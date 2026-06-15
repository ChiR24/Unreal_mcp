#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"

namespace UnrealAgent::AutomationTests
{
FString MakeAcpTestServerSecurityPermissionCases()
{
    return FString()
        + TEXT("    if text == 'ansi c quoted root remove path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"bash -c $'rm -rf .'\"}}\n")
        + TEXT("    if text == 'ansi c quoted git alias path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"git -c $'alias.wipe=!rm -rf .' wipe\"}}\n")
        + TEXT("    if text == 'ansi c escaped git alias path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': r\"git -c $'alias.wipe=\\x21rm -rf .' wipe\"}}\n")
        + TEXT("    if text == 'config env git alias path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"UA_ALIAS='!rm -rf .' git --config-env=alias.wipe=UA_ALIAS wipe\"}}\n")
        + TEXT("    if text == 'ansi c escaped git executable path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': r\"g$'\\x69t' -C . reset --hard\"}}\n")
        + TEXT("    if text == 'ansi c escaped remove executable path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': r\"r$'\\x6d' -rf .\"}}\n")
        + TEXT("    if text == 'git restore path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': 'git restore .'}}\n")
        + TEXT("    if text == 'git checkout path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': 'git checkout .'}}\n")
        + TEXT("    if text == 'nested shell git reset path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"sh -c 'git reset --hard'\"}}\n")
        + TEXT("    if text == 'nested shell git checkout path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': 'bash -lc \"git checkout -- .\"'}}\n")
        + TEXT("    if text == 'stdin shell git reset path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"printf 'git reset --hard\\\\n' | sh\"}}\n")
        + TEXT("    if text == 'encoded stdin shell git reset path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': r\"printf '\\147\\151\\164\\040reset\\040--hard\\n' | sh\"}}\n")
        + TEXT("    if text == 'node child process git reset path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"node -e \\\"require('child_process').execFileSync('git',['reset','--hard'])\\\"\"}}\n")
        + TEXT("    if text == 'encoded stdin content mutation path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': r\"printf '\\155\\153\\144\\151\\162\\040Content/EncodedBypass\\n' | sh\"}}\n")
        + TEXT("    if text == 'escaped git reset path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': r'g\\it -C . reset --hard'}}\n")
        + TEXT("    if text == 'escaped git apply path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': r'git ap\\ply /tmp/policy.patch'}}\n")
        + TEXT("    if text == 'python shutil alias unpack archive path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"python -c \\\"from shutil import unpack_archive as u; u('/tmp/payload.zip', 'Source')\\\"\"}}\n")
        + TEXT("    if text == 'python patool alias extract archive path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"python -c \\\"from patoolib import extract_archive as e; e('/tmp/payload.7z', outdir='Source')\\\"\"}}\n")
        + TEXT("    if text == 'python os alias remove path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"python -c \\\"from os import remove as r; r('Source/Pwn.cpp')\\\"\"}}\n")
        + TEXT("    if text == 'python reordered alias remove path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"python -c \\\"from os import stat, remove as r; r('Source/Pwn.cpp')\\\"\"}}\n")
        + TEXT("    if text == 'python parenthesized alias unpack path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"python -c \\\"from shutil import (copyfile, unpack_archive as u); u('/tmp/payload.zip', 'Source')\\\"\"}}\n")
        + TEXT("    if text == 'python padded tarfile extract path': tool_call = {'toolCallId': 'acp-test-tool', 'status': 'pending', 'title': 'execute_command', 'kind': 'execute', 'rawInput': {'command': \"python -c \\\"import tarfile; padding='\" + ('x' * 400) + \"'; tarfile.open('/tmp/payload.tar').extractall('.')\\\"\"}}\n");
}

FString MakeAcpTestServerPromptRedactionCase()
{
    return FString()
        + TEXT("        if text.startswith('capability_token:'):\n")
        + TEXT("            response_text = 'prompt-redacted-before-acp' if text == 'capability_token: [REDACTED]' else 'prompt-leaked-to-acp'\n")
        + TEXT("            send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'agent_message_chunk', 'content': {'type': 'text', 'text': response_text}}}})\n")
        + TEXT("            send({'jsonrpc': '2.0', 'id': prompt_id, 'result': {'stopReason': 'end_turn'}}); prompt_id = None; continue\n")
        + TEXT("        if text == 'large message burst path':\n")
        + TEXT("            burst_message = {'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'usage_update', 'used': 1, 'size': 64000}}}\n")
        + TEXT("            sys.stdout.write(''.join(json.dumps(burst_message, separators=(',', ':')) + '\\n' for _ in range(9000))); sys.stdout.flush()\n")
        + TEXT("            send({'jsonrpc': '2.0', 'id': prompt_id, 'result': {'stopReason': 'end_turn'}}); prompt_id = None; continue\n")
        + TEXT("        if text == 'oversized complete frame path':\n")
        + TEXT("            oversized_message = {'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'agent_message_chunk', 'content': {'type': 'text', 'text': 'oversized-frame-marker' + ('x' * (1024 * 1024))}}}}\n")
        + TEXT("            sys.stdout.write(json.dumps(oversized_message, separators=(',', ':')) + '\\n'); sys.stdout.flush()\n")
        + TEXT("            send({'jsonrpc': '2.0', 'id': prompt_id, 'result': {'stopReason': 'end_turn'}}); prompt_id = None; continue\n")
        + TEXT("        if text == 'ignore termination path':\n")
        + TEXT("            signal.signal(signal.SIGTERM, signal.SIG_IGN)\n")
        + TEXT("            open(sys.argv[0] + '.pid', 'w').write(str(__import__('os').getpid()))\n")
        + TEXT("            send({'jsonrpc': '2.0', 'id': prompt_id, 'result': {'stopReason': 'end_turn'}}); prompt_id = None; continue\n");
}
}

#endif
