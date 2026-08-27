cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS
        ARCHITECTURE_FILE README_FILE AGENTS_FILE TOKEN_DOC_FILE OPENSPEC_CONFIG_FILE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
    if(NOT EXISTS "${${required_variable}}")
        message(FATAL_ERROR "${required_variable} does not exist")
    endif()
endforeach()

file(READ "${ARCHITECTURE_FILE}" architecture)
file(READ "${README_FILE}" readme)
file(READ "${AGENTS_FILE}" agents)
file(READ "${TOKEN_DOC_FILE}" token_doc)
file(READ "${OPENSPEC_CONFIG_FILE}" openspec_config)

function(require_fragment source label fragment)
    string(FIND "${source}" "${fragment}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${label} is missing required fragment: ${fragment}")
    endif()
endfunction()

require_fragment("${architecture}" architecture "](design-tokens.md)")
require_fragment("${architecture}" architecture "ThemeSnapshot")
require_fragment("${architecture}" architecture "RoundedEffect")
require_fragment("${architecture}" architecture "1 logical px")
require_fragment("${architecture}" architecture "3 logical px")
require_fragment("${readme}" README "[最终架构与实现路线](docs/architecture.md)")
require_fragment("${readme}" README "[Agent 协作规则](AGENTS.md)")
require_fragment("${agents}" AGENTS "README.md")
require_fragment("${agents}" AGENTS "docs/architecture.md")
require_fragment("${agents}" AGENTS "openspec/changes/<change>/")
require_fragment("${token_doc}" token-doc "请勿手工修改")
require_fragment("${token_doc}" token-doc "Token 总数")
require_fragment("${openspec_config}" OpenSpec-config "NNN-YYYYMMDD-lowercase-kebab-case")

foreach(forbidden IN ITEMS
        "OpenSpec 工作流"
        "每完成一个可独立验证的小阶段"
        "Git commit message")
    string(FIND "${readme}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "README duplicates an agent workflow rule: ${forbidden}")
    endif()
endforeach()

string(FIND "${agents}" "RynUI 是一个面向桌面应用的现代 C++ 响应式 UI 框架方案" product_copy)
if(NOT product_copy EQUAL -1)
    message(FATAL_ERROR "AGENTS duplicates the README product introduction")
endif()

foreach(forbidden IN ITEMS
        "| Identity | Category | Kind | Support | Source |"
        "Catalog SHA256：")
    string(FIND "${architecture}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "architecture duplicates generated Token catalog content")
    endif()
endforeach()
