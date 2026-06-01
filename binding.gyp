{
  "targets": [
    {
      "target_name": "node_periphery",
      "sources": [
        "src/addon.cpp",
        "src/gpio/gpio.cpp",
        "src/spi/spi.cc",
        "src/spi/spidevice.cc",
        "src/spi/open.cc",
        "src/spi/close.cc",
        "src/spi/transfer.cc",
        "src/spi/getoptions.cc",
        "src/spi/setoptions.cc"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS"
      ],
      "cflags_cc": [
        "-std=c++17"
      ],
      "conditions": [
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "AdditionalOptions": ["/std:c++17"]
            }
          }
        }],
        ["OS=='mac'", {
          "xcode_settings": {
            "CLANG_CXX_LANGUAGE_STANDARD": "c++17"
          }
        }]
      ]
    }
  ]
}
