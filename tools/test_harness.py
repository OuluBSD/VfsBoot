#!/usr/bin/env python3
"""Stage1 codex-mini integration harness."""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import defaultdict
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
        self._skip_ws()
        if self.pos != self.length:
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
    instructions = _parse_instructions(path, instr_entries)

    prompt_value = expect_single(Symbol('prompt'))
    expected_entries = expect_list(Symbol('expected-output'))
    expected = [_parse_expectation(path, item) for item in expected_entries]

    assertion_entries = expect_list(Symbol('assertions'))
    assertions = [_parse_assertion(path, item) for item in assertion_entries]

    target_entries = expect_list(Symbol('llm-targets'))
    targets = [_parse_target(path, item) for item in target_entries]

    return TestCase(
        id=str(id_value),
        title=str(title_value),
        difficulty=str(difficulty_value),
        tags=tags,
        instructions=instructions,
        prompt=str(prompt_value),
        expected=expected,
        assertions=assertions,
        targets=targets,
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


def call_openai(prompt: str, system_prompt: str, model: Optional[str] = None, base_url: Optional[str] = None) -> str:
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
            {'role': 'system', 'content': [{'type': 'text', 'text': system_prompt}]},
            {'role': 'user', 'content': [{'type': 'text', 'text': prompt}]},
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
                if content.get('type') == 'text':
                    segments.append(content.get('text', ''))
    if not segments:
        raise RuntimeError('OpenAI response missing text content')
    return '\n'.join(segments)


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
    try:
        return call_openai_chat(prompt, system_prompt, base_url, model)
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
            return obj['completion']
        if 'choices' in obj and obj['choices'] and 'text' in obj['choices'][0]:
            return obj['choices'][0]['text']
        raise RuntimeError(f'Unexpected llama completion format: {body}')


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


def run_test_on_target(test: TestCase, target: LlmTarget, binary_path: str) -> Tuple[bool, Dict[str, Any]]:
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
        "Use (comment \"...\") only for optional narration."
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
            if 'error:' in out:
                errors.append(f"command {cmd!r} failed: {out.strip()}")
        cat_paths = collect_assertion_paths(test.assertions)
        for path in cat_paths:
            out = session.run_command(f'cat {path}')
            cat_outputs_entry = (f'cat {path}', out)
            command_outputs.append(cat_outputs_entry)
            if 'error:' in out:
                errors.append(f"cat {path} failed: {out.strip()}")
            cat_results[path] = out
    finally:
        session.close()
    assertion_failures = evaluate_assertions(test.assertions, cat_results)
    errors.extend(assertion_failures)
    success = not errors
    details = {
        'response': response,
        'commands': command_outputs,
        'cat_results': cat_results,
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
    parser = argparse.ArgumentParser(description='Run codex-mini Stage1 harness tests')
    parser.add_argument('paths', nargs='*', default=['tests'], help='Test files or directories to run')
    parser.add_argument('--binary', default='./codex', help='Path to codex binary')
    parser.add_argument('--target', action='append', help='Restrict to specific LLM target kinds (openai, llama)')
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
                success, details = run_test_on_target(case, target, args.binary)
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
