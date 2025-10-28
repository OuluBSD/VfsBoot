#!/bin/bash

# Script to rename header files in VfsShell directory to follow UPP conventions

cd /home/sblo/Dev/VfsBoot/src/VfsShell

# List of files to rename
declare -A header_renames
header_renames["ai_bridge.h"]="AiBridge.h"
header_renames["build_graph.h"]="BuildGraph.h"
header_renames["clang_parser.h"]="ClangParser.h"
header_renames["cmd_qwen.h"]="CmdQwen.h"
header_renames["codex_original.h"]="CodexOriginal.h"
header_renames["command.h"]="Command.h"
header_renames["config.h"]="Config.h"
header_renames["context_builder.h"]="ContextBuilder.h"
header_renames["cpp_ast.h"]="CppAst.h"
header_renames["editor_functions.h"]="EditorFunctions.h"
header_renames["feedback.h"]="Feedback.h"
header_renames["file_browser.h"]="FileBrowser.h"
header_renames["hypothesis.h"]="Hypothesis.h"
header_renames["logic_engine.h"]="LogicEngine.h"
header_renames["make.h"]="Make.h"
header_renames["planner.h"]="Planner.h"
header_renames["qwen_client.h"]="QwenClient.h"
header_renames["qwen_logger.h"]="QwenLogger.h"
header_renames["qwen_manager.h"]="QwenManager.h"
header_renames["qwen_protocol.h"]="QwenProtocol.h"
header_renames["qwen_state_manager.h"]="QwenStateManager.h"
header_renames["qwen_tcp_server.h"]="QwenTcpServer.h"
header_renames["registry.h"]="Registry.h"
header_renames["repl.h"]="Repl.h"
header_renames["scope_store.h"]="ScopeStore.h"
header_renames["sexp.h"]="Sexp.h"
header_renames["shell_commands.h"]="ShellCommands.h"
header_renames["snippet_catalog.h"]="SnippetCatalog.h"
header_renames["tag_system.h"]="TagSystem.h"
header_renames["ui_backend.h"]="UiBackend.h"
header_renames["ui_backend_qt_example.h"]="UiBackendQtExample.h"
header_renames["umk.h"]="Umk.h"
header_renames["upp_assembly.h"]="UppAssembly.h"
header_renames["upp_builder.h"]="UppBuilder.h"
header_renames["upp_toolchain.h"]="UppToolchain.h"
header_renames["upp_workspace_build.h"]="UppWorkspaceBuild.h"
header_renames["utils.h"]="Utils.h"
header_renames["vfs_common.h"]="VfsCommon.h"
header_renames["vfs_common_new.h"]="VfsCommonNew.h"
header_renames["vfs_core.h"]="VfsCore.h"
header_renames["vfs_mount.h"]="VfsMount.h"
header_renames["web_server.h"]="WebServer.h"

# Perform the renaming
for old_name in "${!header_renames[@]}"; do
    new_name=${header_renames[$old_name]}
    if [ -f "$old_name" ]; then
        echo "Renaming $old_name to $new_name"
        mv "$old_name" "$new_name"
    else
        echo "File $old_name does not exist"
    fi
done