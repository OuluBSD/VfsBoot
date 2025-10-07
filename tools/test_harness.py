#!/usr/bin/env python3
"""VfsShell codex-mini integration harness."""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


class Symbol(str):
    """Lightweight marker for S-expression symbols."""


class SExprParser:
    def __init__(self, source: str) -> None:
        self.source = source
        self.length = len(source)
        self.pos = 0

    def parse(self) -> Any:
        node = self._parse_expr()
        while True:
            self._skip_ws()
            if self.pos >= self.length:
                break
            trailing = self._parse_expr()
            if not self._is_comment_form(trailing):
                raise ValueError("unexpected trailing content in S-expression")
        return node

    def _skip_ws(self) -> None:
        while self.pos < self.length:
            ch = self.source[self.pos]
            if ch.isspace():
                self.pos += 1
                continue
            if ch == ';':
                self.pos += 1
                while self.pos < self.length and self.source[self.pos] not in ('\n', '\r'):
                    self.pos += 1
                continue
            break

    def _parse_expr(self) -> Any:
        self._skip_ws()
        if self.pos >= self.length:
            raise ValueError("unexpected end of S-expression")
        ch = self.source[self.pos]
        if ch == '(':
            return self._parse_list()
        if ch == '"':
            return self._parse_string()
        return self._parse_atom()

    def _parse_list(self) -> List[Any]:
        assert self.source[self.pos] == '('
        self.pos += 1
        items: List[Any] = []
        while True:
            self._skip_ws()
            if self.pos >= self.length:
                raise ValueError("unterminated list in S-expression")
            if self.source[self.pos] == ')':
                self.pos += 1
                break
            items.append(self._parse_expr())
        return items

    def _parse_string(self) -> str:
        assert self.source[self.pos] == '"'
        self.pos += 1
        out = []
        while self.pos < self.length:
            ch = self.source[self.pos]
            self.pos += 1
            if ch == '"':
                return ''.join(out)
            if ch == '\\':
                if self.pos >= self.length:
                    raise ValueError("unterminated escape in string literal")
                esc = self.source[self.pos]
                self.pos += 1
                out.append(self._decode_escape(esc))
            else:
                out.append(ch)
        raise ValueError("unterminated string literal")

    @staticmethod
    def _decode_escape(ch: str) -> str:
        mapping = {
            'n': '\n',
            'r': '\r',
            't': '\t',
            'b': '\b',
            'f': '\f',
            'v': '\v',
            'a': '\a',
            '"': '"',
            '\\': '\\',
        }
        return mapping.get(ch, ch)

    def _parse_atom(self) -> Any:
        start = self.pos
        while self.pos < self.length and not self.source[self.pos].isspace() and self.source[self.pos] not in '()':
            self.pos += 1
        token = self.source[start:self.pos]
        if token in ('#t', '#true'):
            return True
        if token in ('#f', '#false'):
            return False
        if re.fullmatch(r'-?[0-9]+', token):
            return int(token)
        return Symbol(token)

    @staticmethod
    def _is_comment_form(expr: Any) -> bool:
        # Permit trailing (comment ...) forms so mildly off-spec assistants do not fail outright.
        return (
            isinstance(expr, list)
            and expr
            and isinstance(expr[0], Symbol)
            and str(expr[0]) == 'comment'
        )


@dataclasses.dataclass
class Expectation:
    kind: str
    values: Tuple[str, ...]


@dataclasses.dataclass
class Assertion:
    kind: str
    path: str
    value: str


@dataclasses.dataclass
class RuntimeExpectation:
    """Expected output from compiled program execution."""
    kind: str
    values: Tuple[str, ...]


@dataclasses.dataclass
class LlmTarget:
    kind: str
    identifier: str


@dataclasses.dataclass
class Instructions:
    format_text: str
    tools: List[str]
    workflow: List[str]

    def render(self) -> str:
        lines = []
        if self.format_text:
            lines.append(f"Format: {self.format_text}")
        if self.tools:
            lines.append("Tools:")
            lines.extend(f"- {item}" for item in self.tools)
        if self.workflow:
            lines.append("Workflow:")
            lines.extend(f"{idx+1}. {step}" for idx, step in enumerate(self.workflow))
        return '\n'.join(lines)


@dataclasses.dataclass
class TestCase:
    id: str
    title: str
    difficulty: str
    tags: List[str]
    instructions: Instructions
    prompt: str
    expected: List[Expectation]
    assertions: List[Assertion]
    targets: List[LlmTarget]
    runtime_expected: List[RuntimeExpectation]
    compile_and_run: bool


_SNIPPET_CACHE: Dict[str, str] = {}
_SNIPPET_DIR: Optional[Path] = None
_SNIPPET_DIR_LOOKED_UP = False
_SNIPPET_PATTERN = re.compile(r"\{\{snippet:([A-Za-z0-9_.-]+)\}\}")


def _fnv1a64(data: bytes) -> int:
    """FNV-1a 64-bit hash matching C++ implementation."""
    hash_val = 0xcbf29ce484222325
    for byte in data:
        hash_val ^= byte
        hash_val = (hash_val * 0x100000001b3) & 0xffffffffffffffff
    return hash_val


def _hash_hex(val: int) -> str:
    """Convert 64-bit integer to hex string."""
    return f"{val:016x}"


def _sanitize_component(s: str) -> str:
    """Sanitize string for use as directory/file name."""
    return re.sub(r'[^A-Za-z0-9._-]', '_', s)


def _ai_cache_root() -> Path:
    """Get the AI cache root directory."""
    env = os.environ.get('CODEX_AI_CACHE_DIR')
    if env:
        return Path(env)
    return Path('cache') / 'ai'


def _make_cache_key_material(provider_signature: str, prompt: str) -> str:
    """Create cache key material from provider signature and prompt."""
    return provider_signature + '\x1f' + prompt


def _ai_cache_paths(provider_label: str, key_material: str) -> Tuple[Path, Path]:
    """Get input and output cache file paths."""
    cache_dir = _ai_cache_root() / _sanitize_component(provider_label)
    hash_str = _hash_hex(_fnv1a64(key_material.encode('utf-8')))
    base = cache_dir / hash_str
    return base.with_suffix('.txt').with_name(f"{hash_str}-in.txt"), base.with_suffix('.txt').with_name(f"{hash_str}-out.txt")


def _ai_cache_read(provider_label: str, key_material: str) -> Optional[str]:
    """Read cached AI response if it exists."""
    in_path, out_path = _ai_cache_paths(provider_label, key_material)
    if out_path.exists():
        return out_path.read_text(encoding='utf-8')
    # Try legacy format
    cache_dir = _ai_cache_root() / _sanitize_component(provider_label)
    hash_str = _hash_hex(_fnv1a64(key_material.encode('utf-8')))
    legacy_path = cache_dir / f"{hash_str}.txt"
    if legacy_path.exists():
        return legacy_path.read_text(encoding='utf-8')
    return None


def _ai_cache_write(provider_label: str, key_material: str, prompt: str, response: str) -> None:
    """Write AI response to cache."""
    in_path, out_path = _ai_cache_paths(provider_label, key_material)
    in_path.parent.mkdir(parents=True, exist_ok=True)
    in_path.write_text(prompt, encoding='utf-8')
    out_path.write_text(response, encoding='utf-8')


def _discover_snippet_dir() -> Optional[Path]:
    global _SNIPPET_DIR_LOOKED_UP, _SNIPPET_DIR
    if _SNIPPET_DIR_LOOKED_UP:
        return _SNIPPET_DIR
    _SNIPPET_DIR_LOOKED_UP = True

    candidates: List[Path] = []
    env = os.environ.get('CODEX_SNIPPET_DIR')
    if env:
        candidates.append(Path(env))
    candidates.append(Path('snippets'))
    candidates.append(Path('VfsShell') / 'snippets')

    for cand in candidates:
        if cand.is_dir():
            _SNIPPET_DIR = cand.resolve()
            return _SNIPPET_DIR
    _SNIPPET_DIR = None
    return None


def _load_snippet(key: str) -> str:
    if key in _SNIPPET_CACHE:
        return _SNIPPET_CACHE[key]
    directory = _discover_snippet_dir()
    if directory is None:
        raise FileNotFoundError(
            "snippet directory not found; set CODEX_SNIPPET_DIR or create VfsShell/snippets"
        )
    path = directory / f'{key}.txt'
    if not path.is_file():
        raise FileNotFoundError(f"snippet '{key}' not found in {directory}")
    text = path.read_text(encoding='utf-8')
    _SNIPPET_CACHE[key] = text
    return text


def _expand_snippet_placeholders(text: str) -> str:
    if not isinstance(text, str) or '{{snippet:' not in text:
        return text

    def repl(match: re.Match) -> str:
        key = match.group(1)
        return _load_snippet(key)

    return _SNIPPET_PATTERN.sub(repl, text)


def _expand_instructions(instr: Instructions) -> Instructions:
    instr.format_text = _expand_snippet_placeholders(instr.format_text)
    instr.tools = [_expand_snippet_placeholders(item) for item in instr.tools]
    instr.workflow = [_expand_snippet_placeholders(step) for step in instr.workflow]
    return instr


def _expand_expectations(items: List[Expectation]) -> List[Expectation]:
    expanded: List[Expectation] = []
    for item in items:
        values = tuple(_expand_snippet_placeholders(value) for value in item.values)
        expanded.append(Expectation(kind=item.kind, values=values))
    return expanded


def _expand_assertions(items: List[Assertion]) -> List[Assertion]:
    expanded: List[Assertion] = []
    for item in items:
        expanded.append(Assertion(kind=item.kind, path=item.path, value=_expand_snippet_placeholders(item.value)))
    return expanded


def _expand_runtime_expectations(items: List[Expectation]) -> List[RuntimeExpectation]:
    expanded: List[RuntimeExpectation] = []
    for item in items:
        values = tuple(_expand_snippet_placeholders(value) for value in item.values)
        expanded.append(RuntimeExpectation(kind=item.kind, values=values))
    return expanded


def load_test_case(path: str) -> TestCase:
    text = open(path, encoding='utf-8').read()
    parsed = SExprParser(text).parse()
    if not isinstance(parsed, list) or not parsed or parsed[0] != Symbol('test-case'):
        raise ValueError(f"{path}: root must be (test-case ...)")
    fields: Dict[str, Any] = {}
    for entry in parsed[1:]:
        if not isinstance(entry, list) or not entry:
            raise ValueError(f"{path}: malformed entry {entry}")
        key = entry[0]
        if not isinstance(key, Symbol):
            raise ValueError(f"{path}: entry without symbol head: {entry}")
        fields[key] = entry[1:]

    def expect_single(name: str) -> Any:
        if name not in fields or len(fields[name]) != 1:
            raise ValueError(f"{path}: expected single value for {name}")
        return fields[name][0]

    def expect_list(name: str) -> List[Any]:
        if name not in fields:
            raise ValueError(f"{path}: missing {name}")
        return list(fields[name])

    id_value = expect_single(Symbol('id'))
    title_value = expect_single(Symbol('title'))
    difficulty_value = expect_single(Symbol('difficulty'))
    tags_expr = expect_single(Symbol('tags'))
    if not isinstance(tags_expr, list):
        raise ValueError(f"{path}: tags must be list")
    tags = [str(x) for x in tags_expr]

    instr_entries = expect_list(Symbol('instructions'))
    instructions = _expand_instructions(_parse_instructions(path, instr_entries))

    prompt_value = expect_single(Symbol('prompt'))
    expected_entries = expect_list(Symbol('expected-output'))
    expected = _expand_expectations([_parse_expectation(path, item) for item in expected_entries])

    assertion_entries = expect_list(Symbol('assertions'))
    assertions = _expand_assertions([_parse_assertion(path, item) for item in assertion_entries])

    target_entries = expect_list(Symbol('llm-targets'))
    targets = [_parse_target(path, item) for item in target_entries]

    # Optional: runtime expectations
    runtime_expected: List[RuntimeExpectation] = []
    compile_and_run = False
    if Symbol('expected-runtime-output') in fields:
        runtime_entries = fields[Symbol('expected-runtime-output')]
        runtime_expected = _expand_runtime_expectations([_parse_expectation(path, item) for item in runtime_entries])
        compile_and_run = True
    if Symbol('compile-and-run') in fields:
        compile_flag = fields[Symbol('compile-and-run')][0]
        compile_and_run = compile_flag in ('#t', '#true', True)

    return TestCase(
        id=str(id_value),
        title=str(title_value),
        difficulty=str(difficulty_value),
        tags=tags,
        instructions=instructions,
        prompt=_expand_snippet_placeholders(str(prompt_value)),
        expected=expected,
        assertions=assertions,
        targets=targets,
        runtime_expected=runtime_expected,
        compile_and_run=compile_and_run,
    )


def _parse_instructions(path: str, entries: Sequence[Any]) -> Instructions:
    format_text = ''
    tools: List[str] = []
    workflow: List[str] = []
    for entry in entries:
        if not isinstance(entry, list) or not entry:
            raise ValueError(f"{path}: malformed instructions entry {entry}")
        head = entry[0]
        if head == Symbol('format'):
            if len(entry) != 2:
                raise ValueError(f"{path}: format expects single string")
            format_text = str(entry[1])
        elif head == Symbol('tools'):
            for tool_entry in entry[1:]:
                if isinstance(tool_entry, list) and tool_entry and tool_entry[0] == Symbol('item'):
                    if len(tool_entry) != 2:
                        raise ValueError(f"{path}: tool item must have single value")
                    tools.append(str(tool_entry[1]))
                else:
                    tools.append(str(tool_entry))
        elif head == Symbol('workflow'):
            for step_entry in entry[1:]:
                if isinstance(step_entry, list) and step_entry and step_entry[0] == Symbol('step'):
                    if len(step_entry) != 2:
                        raise ValueError(f"{path}: workflow step must have single value")
                    workflow.append(str(step_entry[1]))
                else:
                    workflow.append(str(step_entry))
        else:
            raise ValueError(f"{path}: unknown instructions key {head}")
    return Instructions(format_text=format_text, tools=tools, workflow=workflow)


def _parse_expectation(path: str, expr: Any) -> Expectation:
    if not isinstance(expr, list) or not expr:
        raise ValueError(f"{path}: expected-output entry malformed: {expr}")
    head = expr[0]
    if not isinstance(head, Symbol):
        raise ValueError(f"{path}: expected-output head must be symbol")
    values = tuple(str(x) for x in expr[1:])
    return Expectation(kind=head, values=values)


def _parse_assertion(path: str, expr: Any) -> Assertion:
    if not isinstance(expr, list) or len(expr) != 3:
        raise ValueError(f"{path}: assertion must be (kind path value)")
    kind = expr[0]
    if not isinstance(kind, Symbol):
        raise ValueError(f"{path}: assertion kind must be symbol")
    path_value = str(expr[1])
    value = str(expr[2])
    return Assertion(kind=str(kind), path=path_value, value=value)


def _parse_target(path: str, expr: Any) -> LlmTarget:
    if not isinstance(expr, list) or len(expr) != 2:
        raise ValueError(f"{path}: llm-target entry malformed: {expr}")
    kind = expr[0]
    if not isinstance(kind, Symbol):
        raise ValueError(f"{path}: target kind must be symbol")
    identifier = str(expr[1])
    return LlmTarget(kind=str(kind), identifier=identifier)


class CodexSession:
    def __init__(self, binary_path: str) -> None:
        self.proc = subprocess.Popen(
            [binary_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self._read_until_prompt()  # splash + first prompt

    def _read_until_prompt(self) -> str:
        assert self.proc.stdout is not None
        buffer: List[str] = []
        while True:
            chunk = self.proc.stdout.read(1)
            if chunk == '':
                raise RuntimeError('codex terminated unexpectedly while waiting for prompt')
            buffer.append(chunk)
            if len(buffer) >= 3 and buffer[-3:] == ['\n', '>', ' ']:
                text = ''.join(buffer[:-3])
                return text

    def run_command(self, command: str) -> str:
        if self.proc.stdin is None:
            raise RuntimeError('codex stdin unavailable')
        self.proc.stdin.write(command + '\n')
        self.proc.stdin.flush()
        return self._read_until_prompt()

    def close(self) -> Tuple[str, str]:
        if self.proc.stdin is not None:
            try:
                self.proc.stdin.write('exit\n')
                self.proc.stdin.flush()
            except BrokenPipeError:
                pass
        remaining_out = ''
        if self.proc.stdout is not None:
            remaining_out = self.proc.stdout.read() or ''
        remaining_err = ''
        if self.proc.stderr is not None:
            remaining_err = self.proc.stderr.read() or ''
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        return remaining_out, remaining_err


def _is_linux() -> bool:
    """Check if running on Linux."""
    return platform.system() == 'Linux'


def _compile_cpp_file(source_path: Path, output_path: Path, timeout: int = 30) -> Tuple[bool, str, str]:
    """
    Compile a C++ source file.
    Returns (success, stdout, stderr).
    """
    compiler = os.environ.get('CXX', 'g++')
    cmd = [compiler, '-std=c++17', '-O2', '-o', str(output_path), str(source_path)]
    try:
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout
        )
        success = result.returncode == 0
        return success, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, '', f'Compilation timed out after {timeout}s'
    except Exception as e:
        return False, '', f'Compilation error: {e}'


def _run_sandboxed(executable_path: Path, timeout: int = 10) -> Tuple[bool, str, str, int]:
    """
    Run executable in a sandbox (Gentoo-style on Linux, basic on Windows).
    Returns (success, stdout, stderr, returncode).
    """
    if _is_linux():
        # Try Linux sandbox with unshare for namespace isolation
        cmd = [
            'timeout', str(timeout),
            'unshare', '--net', '--pid', '--fork',
            str(executable_path)
        ]
        fallback_cmd = ['timeout', str(timeout), str(executable_path)]

        try:
            result = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=timeout + 5  # Extra buffer for cleanup
            )
            # If unshare failed (permission denied), fall back
            if result.returncode != 0 and ('unshare' in result.stderr or 'Toiminto ei ole sallittu' in result.stderr):
                result = subprocess.run(
                    fallback_cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    timeout=timeout
                )
            return result.returncode == 0, result.stdout, result.stderr, result.returncode
        except FileNotFoundError:
            # unshare or timeout not available, use fallback
            result = subprocess.run(
                fallback_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=timeout
            )
            return result.returncode == 0, result.stdout, result.stderr, result.returncode
        except subprocess.TimeoutExpired:
            return False, '', f'Execution timed out after {timeout}s', -1
        except Exception as e:
            return False, '', f'Execution error: {e}', -1
    else:
        # Windows: just run directly with timeout
        cmd = [str(executable_path)]
        try:
            result = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=timeout
            )
            return result.returncode == 0, result.stdout, result.stderr, result.returncode
        except subprocess.TimeoutExpired:
            return False, '', f'Execution timed out after {timeout}s', -1
        except Exception as e:
            return False, '', f'Execution error: {e}', -1


def build_prompt_text(test: TestCase) -> str:
    parts = [
        f"Test {test.id}: {test.title}",
        f"Difficulty: {test.difficulty}",
    ]
    if test.tags:
        parts.append("Tags: " + ', '.join(test.tags))
    instr = test.instructions.render()
    if instr:
        parts.append(instr)
    parts.append("Task:")
    parts.append(test.prompt)
    return '\n\n'.join(parts)


def _openai_cache_signature(model: Optional[str] = None, base_url: Optional[str] = None) -> str:
    """Build OpenAI cache signature matching C++ implementation."""
    base = base_url or os.environ.get('OPENAI_BASE_URL', 'https://api.openai.com/v1')
    base = base.rstrip('/')
    mdl = model or os.environ.get('OPENAI_MODEL', 'gpt-4o-mini')
    return f"openai|{mdl}|{base}"


def _llama_cache_signature(base_url: str, model: str) -> str:
    """Build Llama cache signature matching C++ implementation."""
    base = base_url.rstrip('/')
    return f"llama|{model}|{base}"


def call_openai(prompt: str, system_prompt: str, model: Optional[str] = None, base_url: Optional[str] = None) -> str:
    # Build cache signature and check cache
    signature = _openai_cache_signature(model, base_url)
    full_prompt = f"{system_prompt}\n\n{prompt}"
    key_material = _make_cache_key_material(signature, full_prompt)

    cached = _ai_cache_read('openai', key_material)
    if cached is not None:
        return cached

    # Cache miss - make actual API call
    api_key = os.environ.get('OPENAI_API_KEY')
    if not api_key:
        raise RuntimeError('OPENAI_API_KEY not set')
    model = model or os.environ.get('OPENAI_MODEL', 'gpt-4o-mini')
    base = base_url or os.environ.get('OPENAI_BASE_URL', 'https://api.openai.com/v1')
    if base.endswith('/'):
        base = base[:-1]
    url = base + '/responses'
    headers = {
        'Authorization': f'Bearer {api_key}',
        'Content-Type': 'application/json',
    }
    payload = {
        'model': model,
        'input': [
            {'role': 'system', 'content': [{'type': 'input_text', 'text': system_prompt}]},
            {'role': 'user', 'content': [{'type': 'input_text', 'text': prompt}]},
        ],
        'temperature': 0.0,
    }
    data = json.dumps(payload).encode('utf-8')
    request = urllib.request.Request(url, data=data, headers=headers, method='POST')
    try:
        with urllib.request.urlopen(request, timeout=60) as resp:
            body = resp.read().decode('utf-8')
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode('utf-8', errors='replace')
        raise RuntimeError(f'OpenAI request failed: {exc.code} {exc.reason}: {detail}') from exc
    except urllib.error.URLError as exc:
        raise RuntimeError(f'OpenAI request failed: {exc}') from exc
    obj = json.loads(body)
    if 'output' not in obj:
        raise RuntimeError(f'Unexpected OpenAI response: {body}')
    segments = []
    for item in obj['output']:
        if item.get('type') == 'message':
            for content in item.get('content', []):
                if content.get('type') == 'output_text':
                    segments.append(content.get('text', ''))
    if not segments:
        raise RuntimeError('OpenAI response missing text content')
    response = '\n'.join(segments)

    # Write to cache
    _ai_cache_write('openai', key_material, full_prompt, response)
    return response


def call_openai_chat(prompt: str, system_prompt: str, base_url: str, model: str) -> str:
    url = base_url.rstrip('/') + '/v1/chat/completions'
    headers = {'Content-Type': 'application/json'}
    payload = {
        'model': model,
        'messages': [
            {'role': 'system', 'content': system_prompt},
            {'role': 'user', 'content': prompt},
        ],
        'temperature': 0.0,
    }
    data = json.dumps(payload).encode('utf-8')
    request = urllib.request.Request(url, data=data, headers=headers, method='POST')
    with urllib.request.urlopen(request, timeout=60) as resp:
        body = resp.read().decode('utf-8')
    obj = json.loads(body)
    choices = obj.get('choices')
    if not choices:
        raise RuntimeError(f'LLM response missing choices: {body}')
    content = choices[0].get('message', {}).get('content')
    if not isinstance(content, str):
        raise RuntimeError(f'LLM response missing message content: {body}')
    return content


def call_llama(prompt: str, system_prompt: str, base_url: str, model: str) -> str:
    # Build cache signature and check cache
    signature = _llama_cache_signature(base_url, model)
    full_prompt = f"{system_prompt}\n\n{prompt}"
    key_material = _make_cache_key_material(signature, full_prompt)

    cached = _ai_cache_read('llama', key_material)
    if cached is not None:
        return cached

    # Cache miss - make actual API call
    try:
        response = call_openai_chat(prompt, system_prompt, base_url, model)
    except Exception:
        url = base_url.rstrip('/') + '/completion'
        payload = {
            'prompt': f"<|system|>\n{system_prompt}\n<|user|>\n{prompt}\n<|assistant|>",
            'temperature': 0.0,
            'stream': False,
        }
        data = json.dumps(payload).encode('utf-8')
        request = urllib.request.Request(url, data=data, headers={'Content-Type': 'application/json'}, method='POST')
        with urllib.request.urlopen(request, timeout=60) as resp:
            body = resp.read().decode('utf-8')
        obj = json.loads(body)
        if 'completion' in obj:
            response = obj['completion']
        elif 'choices' in obj and obj['choices'] and 'text' in obj['choices'][0]:
            response = obj['choices'][0]['text']
        else:
            raise RuntimeError(f'Unexpected llama completion format: {body}')

    # Write to cache
    _ai_cache_write('llama', key_material, full_prompt, response)
    return response


def ensure_begin_commands(expr: Any) -> List[List[Any]]:
    if not isinstance(expr, list) or not expr or expr[0] != Symbol('begin'):
        raise ValueError('LLM response must be (begin ...)')
    commands: List[List[Any]] = []
    for item in expr[1:]:
        if not isinstance(item, list) or not item:
            raise ValueError(f'malformed form inside begin: {item}')
        head = item[0]
        if head == Symbol('comment'):
            continue
        if head != Symbol('cmd'):
            raise ValueError(f'unsupported form {head}, expected (cmd ...)')
        commands.append(item)
    return commands


def stringify_argument(arg: Any) -> str:
    if isinstance(arg, bool):
        return '#t' if arg else '#f'
    if isinstance(arg, (int, float)):
        return str(arg)
    if isinstance(arg, Symbol):
        return str(arg)
    if isinstance(arg, str):
        return (
            arg
            .replace('\r', '\\r')
            .replace('\n', '\\n')
            .replace('\t', '\\t')
        )
    raise TypeError(f'unhandled argument type: {arg!r}')


def commands_from_forms(forms: Sequence[List[Any]]) -> List[str]:
    commands: List[str] = []
    for form in forms:
        if len(form) < 2:
            raise ValueError(f'malformed cmd form: {form}')
        tool = form[1]
        if not isinstance(tool, str):
            raise ValueError(f'cmd tool must be string, got {tool!r}')
        args = [stringify_argument(arg) for arg in form[2:]]
        line = ' '.join([tool] + args)
        if tool in ('exit', 'quit'):
            continue
        commands.append(line)
    return commands


def validate_expected(response: str, expected: Sequence[Expectation]) -> List[str]:
    failures: List[str] = []
    for exp in expected:
        if exp.kind == 'contains':
            for value in exp.values:
                if value not in response:
                    failures.append(f"missing expected snippet: {value!r}")
        elif exp.kind == 'not-contains':
            for value in exp.values:
                if value in response:
                    failures.append(f"unexpected snippet present: {value!r}")
        else:
            failures.append(f"unsupported expectation kind: {exp.kind}")
    return failures


def collect_assertion_paths(assertions: Sequence[Assertion]) -> List[str]:
    seen: Dict[str, None] = {}
    order: List[str] = []
    for assertion in assertions:
        if assertion.path not in seen:
            seen[assertion.path] = None
            order.append(assertion.path)
    return order


def evaluate_assertions(assertions: Sequence[Assertion], cat_outputs: Dict[str, str]) -> List[str]:
    failures: List[str] = []
    for assertion in assertions:
        content = cat_outputs.get(assertion.path)
        if content is None:
            failures.append(f"missing content for {assertion.path}")
            continue
        if assertion.kind == 'contains':
            if assertion.value not in content:
                failures.append(f"{assertion.path} missing {assertion.value!r}")
        elif assertion.kind == 'not-contains':
            if assertion.value in content:
                failures.append(f"{assertion.path} unexpectedly contains {assertion.value!r}")
        else:
            failures.append(f"unsupported assertion kind: {assertion.kind}")
    return failures


def validate_runtime_output(output: str, expected: Sequence[RuntimeExpectation]) -> List[str]:
    """Validate runtime output from compiled executable."""
    failures: List[str] = []
    for exp in expected:
        if exp.kind == 'contains':
            for value in exp.values:
                if value not in output:
                    failures.append(f"runtime output missing expected snippet: {value!r}")
        elif exp.kind == 'not-contains':
            for value in exp.values:
                if value in output:
                    failures.append(f"runtime output unexpectedly contains: {value!r}")
        elif exp.kind == 'equals':
            if exp.values:
                expected_text = exp.values[0]
                if output.strip() != expected_text.strip():
                    failures.append(f"runtime output mismatch:\nExpected: {expected_text!r}\nGot: {output!r}")
        else:
            failures.append(f"unsupported runtime expectation kind: {exp.kind}")
    return failures


def run_test_on_target(test: TestCase, target: LlmTarget, binary_path: str, *, verbose: bool = False) -> Tuple[bool, Dict[str, Any]]:
    system_prompt = (
        "Respond with exactly one S-expression matching this template.\n"
        "(begin\n"
        "  (cmd \"tool\" arg...)\n"
        "  ...\n"
        ")\n"
        "Every actionable step MUST be wrapped as (cmd \"tool\" ...).\n"
        "Never emit bare tool forms like (vfs-write ...) or (ls ...); wrap them in cmd.\n"
        "Follow the task instructions literally, executing the suggested workflow steps in order.\n"
        "If the instructions mention running (cmd \"tools\") or similar setup commands, include them before other actions.\n"
        "Use (comment \"...\") only for optional narration.\n"
        "Output MUST be a raw S-expression without markdown fences or extra prose.\n"
        "Execute every workflow step exactly once, in the listed order, even if it feels redundant, and avoid injecting unrelated commands.\n"
        "Use the precise paths provided by the prompt and expectations (e.g. /astcpp/tests/hello) without inventing extra suffixes.\n"
        "Create translation units at the exact path requested (e.g. /astcpp/tests/hello) and nest functions beneath them (e.g. /astcpp/tests/hello/main).\n"
        "For cpp.include pass the header name without angle brackets and set the trailing flag to 1 for angled includes—never 0 when using <...> headers.\n"
        "For cpp.print provide the literal message text once, without additional escaping or quotes.\n"
        "When referencing AST nodes after creation, reuse their absolute VFS paths (e.g. /astcpp/tests/hello/main) rather than bare identifiers.\n"
        "After cpp.dump, run cat on the generated file path to verify its contents when the workflow mentions verification.\n"
        "Do not create auxiliary directories with mkdir unless the workflow requires it.\n"
        "Invoke cpp.tu with the translation unit path alone (e.g. /astcpp/tests/hello) and reserve child paths (e.g. /astcpp/tests/hello/main) for functions."
        "Conclude with the exact final form (comment \"std::cout handles the greeting\")."
    )
    prompt = build_prompt_text(test)
    if target.kind == 'openai':
        response = call_openai(prompt, system_prompt)
    elif target.kind == 'llama':
        llama_model = os.environ.get('LLAMA_MODEL', 'codex-mini')
        response = call_llama(prompt, system_prompt, target.identifier, llama_model)
    else:
        raise RuntimeError(f'unsupported llm target kind: {target.kind}')
    response = response.strip()
    if verbose:
        print("--- LLM response ---")
        print(response)
        print("--- end response ---")
    expected_failures = validate_expected(response, test.expected)
    if expected_failures:
        return False, {'response': response, 'errors': expected_failures}
    parsed = SExprParser(response).parse()
    forms = ensure_begin_commands(parsed)
    commands = commands_from_forms(forms)
    session = CodexSession(binary_path)
    command_outputs: List[Tuple[str, str]] = []
    errors: List[str] = []
    cat_results: Dict[str, str] = {}
    try:
        for cmd in commands:
            out = session.run_command(cmd)
            command_outputs.append((cmd, out))
            if verbose:
                print(f"$ {cmd}")
                if out.strip():
                    print(out.rstrip())
                else:
                    print("(no output)")
            if 'error:' in out:
                errors.append(f"command {cmd!r} failed: {out.strip()}")
        cat_paths = collect_assertion_paths(test.assertions)
        for path in cat_paths:
            cat_cmd = f'cat {path}'
            out = session.run_command(cat_cmd)
            cat_outputs_entry = (f'cat {path}', out)
            command_outputs.append(cat_outputs_entry)
            if verbose:
                print(f"$ {cat_cmd}")
                if out.strip():
                    print(out.rstrip())
                else:
                    print("(no output)")
            if 'error:' in out:
                errors.append(f"cat {path} failed: {out.strip()}")
            cat_results[path] = out
    finally:
        session.close()

    assertion_failures = evaluate_assertions(test.assertions, cat_results)
    errors.extend(assertion_failures)

    # Compile and run if requested
    runtime_output = ''
    compile_stdout = ''
    compile_stderr = ''
    run_stdout = ''
    run_stderr = ''
    if test.compile_and_run and not errors:
        # Find C++ files in /cpp directory from cat_results
        cpp_files: List[Tuple[str, str]] = []
        for path, content in cat_results.items():
            if path.startswith('/cpp/') and path.endswith('.cpp'):
                cpp_files.append((path, content))

        if cpp_files and not errors:
            # For now, compile the first .cpp file found
            vfs_path, source_content = cpp_files[0]

            with tempfile.TemporaryDirectory() as tmpdir:
                tmpdir_path = Path(tmpdir)
                source_file = tmpdir_path / 'program.cpp'
                executable = tmpdir_path / 'program'

                # Write source to temp file
                source_file.write_text(source_content, encoding='utf-8')

                # Compile
                compile_success, compile_stdout, compile_stderr = _compile_cpp_file(source_file, executable)

                if verbose:
                    print(f"--- Compiling {vfs_path} ---")
                    if compile_stdout:
                        print("stdout:", compile_stdout)
                    if compile_stderr:
                        print("stderr:", compile_stderr)

                if not compile_success:
                    errors.append(f"Compilation failed for {vfs_path}")
                    if compile_stderr:
                        errors.append(f"Compiler errors: {compile_stderr}")
                else:
                    # Run the executable
                    run_success, run_stdout, run_stderr, returncode = _run_sandboxed(executable)
                    runtime_output = run_stdout

                    if verbose:
                        print(f"--- Running {vfs_path} ---")
                        print("stdout:", run_stdout)
                        if run_stderr:
                            print("stderr:", run_stderr)
                        print(f"return code: {returncode}")

                    # Validate runtime output
                    if test.runtime_expected:
                        runtime_failures = validate_runtime_output(runtime_output, test.runtime_expected)
                        errors.extend(runtime_failures)

    success = not errors
    details = {
        'response': response,
        'commands': command_outputs,
        'cat_results': cat_results,
        'compile_stdout': compile_stdout,
        'compile_stderr': compile_stderr,
        'runtime_stdout': run_stdout,
        'runtime_stderr': run_stderr,
        'errors': errors,
    }
    return success, details


def discover_tests(paths: Sequence[str]) -> List[str]:
    collected: List[str] = []
    for entry in paths:
        if os.path.isdir(entry):
            for root, _, files in os.walk(entry):
                for name in files:
                    if name.endswith('.sexp'):
                        collected.append(os.path.join(root, name))
        else:
            collected.append(entry)
    return sorted(collected)


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description='Run codex-mini VfsShell harness tests')
    parser.add_argument('paths', nargs='*', default=['tests'], help='Test files or directories to run')
    parser.add_argument('--binary', default='./codex', help='Path to codex binary')
    parser.add_argument('--target', action='append', help='Restrict to specific LLM target kinds (openai, llama)')
    parser.add_argument('-v', '--verbose', action='store_true', help='Log LLM responses and command output during tests')
    args = parser.parse_args(argv)

    tests = discover_tests(args.paths)
    if not tests:
        print('No tests found', file=sys.stderr)
        return 1

    target_filter = set(args.target or [])
    overall_success = True
    for test_path in tests:
        try:
            case = load_test_case(test_path)
        except Exception as exc:
            print(f"[LOAD-ERROR] {test_path}: {exc}", file=sys.stderr)
            overall_success = False
            continue
        print(f"=== {case.id} ({test_path}) ===")
        for target in case.targets:
            if target_filter and target.kind not in target_filter:
                continue
            start = time.time()
            try:
                success, details = run_test_on_target(case, target, args.binary, verbose=args.verbose)
                status = 'PASS' if success else 'FAIL'
                duration = time.time() - start
                print(f"[{status}] {target.kind} {target.identifier} ({duration:.2f}s)")
                if not success:
                    overall_success = False
                    for err in details['errors']:
                        print(f"  - {err}")
            except Exception as exc:
                overall_success = False
                print(f"[ERROR] {target.kind} {target.identifier}: {exc}", file=sys.stderr)
        print()
    return 0 if overall_success else 2


if __name__ == '__main__':
    sys.exit(main())
