# EmbedCatalogue.cmake
#
# Embeds a tool-catalogue JSON file into a generated C header as a string
# constant, so the catalogue ships inside the binary with no runtime file I/O
# (works sandboxed and on ESP32) - see TASK-C2.6a, 16-TOOLS.md.
#
# ethervox_embed_catalogue(<json_path> <c_identifier> <output_header_path>)

function(ethervox_embed_catalogue JSON_PATH C_IDENTIFIER OUTPUT_HEADER)
    file(READ "${JSON_PATH}" _catalogue_json_content)

    # Escape for a C string literal: backslash, then quote, then turn each
    # newline into a closing+reopening quote so long files stay one literal
    # made of adjacent string-literal pieces (which C concatenates).
    string(REPLACE "\\" "\\\\" _catalogue_json_content "${_catalogue_json_content}")
    string(REPLACE "\"" "\\\"" _catalogue_json_content "${_catalogue_json_content}")
    string(REPLACE "\n" "\\n\"\n\"" _catalogue_json_content "${_catalogue_json_content}")

    set(_header_content "// Generated from ${JSON_PATH} by EmbedCatalogue.cmake - do not edit.\n")
    string(APPEND _header_content "#pragma once\n\n")
    string(APPEND _header_content "static const char* const ${C_IDENTIFIER} =\n\"${_catalogue_json_content}\"\n;\n")

    file(WRITE "${OUTPUT_HEADER}" "${_header_content}")
endfunction()
