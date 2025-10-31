#!/bin/bash

# Script to rename source files in VfsShell directory to follow UPP conventions

cd /home/sblo/Dev/VfsBoot/src/VfsShell

# List of files to rename
declare -A source_renames
source_renames["ai_bridge.cpp"]="AiBridge.cpp"
source_renames["build_graph.cpp"]="BuildGraph.cpp"
source_renames["clang_parser.cpp"]="ClangParser.cpp"
source_renames["cmd_qwen.cpp"]="CmdQwen.cpp"
source_renames["command.cpp"]="Command.cpp"
source_renames["context_builder.cpp"]="ContextBuilder.cpp"
source_renames["cpp_ast.cpp"]="CppAst.cpp"
source_renames["daemon.cpp"]="Daemon.cpp"
source_renames["feedback.cpp"]="Feedback.cpp"
source_renames["hypothesis.cpp"]="Hypothesis.cpp"
source_renames["logic_engine.cpp"]="LogicEngine.cpp"
source_renames["main.cpp"]="Main.cpp"
source_renames["make.cpp"]="Make.cpp"
source_renames["planner.cpp"]="Planner.cpp"
source_renames["qwen_client.cpp"]="QwenClient.cpp"
source_renames["qwen_client_test.cpp"]="QwenClientTest.cpp"
source_renames["qwen_echo_server.cpp"]="QwenEchoServer.cpp"
source_renames["qwen_integration_test.cpp"]="QwenIntegrationTest.cpp"
source_renames["qwen_manager.cpp"]="QwenManager.cpp"
source_renames["qwen_protocol.cpp"]="QwenProtocol.cpp"
source_renames["qwen_protocol_test.cpp"]="QwenProtocolTest.cpp"
source_renames["qwen_state_manager.cpp"]="QwenStateManager.cpp"
source_renames["qwen_state_manager_test.cpp"]="QwenStateManagerTest.cpp"
source_renames["qwen_tcp_server.cpp"]="QwenTcpServer.cpp"
source_renames["registry.cpp"]="Registry.cpp"
source_renames["repl.cpp"]="Repl.cpp"
source_renames["scope_store.cpp"]="ScopeStore.cpp"
source_renames["sexp.cpp"]="Sexp.cpp"
source_renames["shell_commands.cpp"]="ShellCommands.cpp"
source_renames["snippet_catalog.cpp"]="SnippetCatalog.cpp"
source_renames["tag_system.cpp"]="TagSystem.cpp"
source_renames["umk.cpp"]="Umk.cpp"
source_renames["updated_make_command_for_package.cpp"]="UpdatedMakeCommandForPackage.cpp"
source_renames["upp_assembly.cpp"]="UppAssembly.cpp"
source_renames["upp_builder.cpp"]="UppBuilder.cpp"
source_renames["upp_toolchain.cpp"]="UppToolchain.cpp"
source_renames["upp_workspace_build.cpp"]="UppWorkspaceBuild.cpp"
source_renames["utils.cpp"]="Utils.cpp"
source_renames["vfs_common.cpp"]="VfsCommon.cpp"
source_renames["vfs_core.cpp"]="VfsCore.cpp"
source_renames["vfs_mount.cpp"]="VfsMount.cpp"
source_renames["web_server.cpp"]="WebServer.cpp"

# Perform the renaming
for old_name in "${!source_renames[@]}"; do
    new_name=${source_renames[$old_name]}
    if [ -f "$old_name" ]; then
        echo "Renaming $old_name to $new_name"
        mv "$old_name" "$new_name"
    else
        echo "File $old_name does not exist"
    fi
done