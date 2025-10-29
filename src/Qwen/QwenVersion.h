#ifndef _Qwen_QwenVersion_h_
#define _Qwen_QwenVersion_h_

// Expected qwen-code protocol version
// This should match the version in ~/Dev/qwen-code/package.json
//
// Note: The actual version is read dynamically from qwen-code's init message
// in QwenProtocol.cpp. This constant is only used for test/mock scenarios
// where we simulate qwen-code responses.
#define QWEN_PROTOCOL_VERSION "0.1.1"

#endif // _Qwen_QwenVersion_h_
