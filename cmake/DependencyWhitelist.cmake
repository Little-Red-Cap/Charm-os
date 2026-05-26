function(charm_check_imports files disallow_regex message)
    foreach(path IN LISTS files)
        if (NOT EXISTS "${path}")
            continue()
        endif()
        file(STRINGS "${path}" lines ENCODING UTF-8)
        set(line_number 0)
        foreach(line IN LISTS lines)
            math(EXPR line_number "${line_number} + 1")
            string(REGEX MATCH "${disallow_regex}" match "${line}")
            if (match)
                string(STRIP "${line}" stripped_line)
                message(FATAL_ERROR "${message}: ${path}:${line_number}\n  ${stripped_line}")
            endif()
        endforeach()
    endforeach()
endfunction()

function(charm_require_files files message)
    foreach(path IN LISTS files)
        if (NOT EXISTS "${path}")
            message(FATAL_ERROR "${message}: ${path}")
        endif()
    endforeach()
endfunction()

function(charm_check_entry_inventory entry_files classified_files)
    foreach(path IN LISTS entry_files)
        if (NOT "${path}" IN_LIST classified_files)
            message(FATAL_ERROR
                "Unclassified charm.* entry module: ${path}\n"
                "  Classify it in cmake/DependencyWhitelist.cmake and docs/architecture/dependency_whitelist.md")
        endif()
    endforeach()
endfunction()

function(charm_enforce_dependency_whitelist)
    set(first_party_roots
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules"
        "${CMAKE_CURRENT_SOURCE_DIR}/Examples"
        "${CMAKE_CURRENT_SOURCE_DIR}/Draft")
    set(first_party_source_extensions
        cpp cxx cc
        hpp hxx hh h
        cppm ixx mpp mxx)
    set(first_party_entry_globs "")
    foreach(root IN LISTS first_party_roots)
        foreach(extension IN LISTS first_party_source_extensions)
            list(APPEND first_party_entry_globs "${root}/*.${extension}")
        endforeach()
    endforeach()

    set(first_party_entry_files "")
    file(GLOB_RECURSE first_party_entry_files CONFIGURE_DEPENDS
        ${first_party_entry_globs})
    list(FILTER first_party_entry_files EXCLUDE REGEX "[/\\\\]cmake-build-[^/\\\\]+[/\\\\]")
    list(FILTER first_party_entry_files EXCLUDE REGEX "[/\\\\]Modules[/\\\\]thirdparty[/\\\\]")

    set(stable_entry_files
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/core/charm.core.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/system/charm.system.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/io/charm.io.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/io/charm.net.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/media/charm.media.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/media/charm.media.audio.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/ink/charm.ui.ink.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/charm.ui.vivid.cppm")
    charm_require_files(
        "${stable_entry_files}"
        "Stable entry module listed in dependency whitelist is missing")

    set(non_stable_entry_files
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/core/charm.foundation.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/system/charm.runtime.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/common/charm.core.event.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/charm.ui.vivid_internal.cppm")

    set(classified_entry_files
        ${stable_entry_files}
        ${non_stable_entry_files})

    set(all_charm_entry_files "")
    file(GLOB_RECURSE all_charm_entry_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/charm.*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*/charm.*.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*/*/charm.*.cppm")
    list(FILTER all_charm_entry_files EXCLUDE REGEX "[/\\\\]cmake-build-[^/\\\\]+[/\\\\]")
    list(FILTER all_charm_entry_files EXCLUDE REGEX "[/\\\\]Modules[/\\\\]thirdparty[/\\\\]")
    charm_check_entry_inventory(
        "${all_charm_entry_files}"
        "${classified_entry_files}")

    set(entry_exports_retired "^[ \t]*export[ \t]+import[ \t]+charm\\.(foundation|runtime|domain)([^A-Za-z0-9_.]|$)")
    charm_check_imports(
        "${stable_entry_files}"
        "${entry_exports_retired}"
        "Stable entry module re-exported historical entry")

    set(entry_exports_private "^[ \t]*export[ \t]+import[ \t]+[^;]*(internal|bridge|compat|alias)[^;]*;")
    charm_check_imports(
        "${stable_entry_files}"
        "${entry_exports_private}"
        "Stable entry module re-exported private or compatibility surface")

    set(import_removed "^[ \t]*(export[ \t]+)?import[ \t]+charm\\.(foundation|runtime|domain)([^A-Za-z0-9_.]|$)")

    charm_check_imports(
        "${first_party_entry_files}"
        "${import_removed}"
        "Historical or compatibility entry module imported")

    set(kernel_core_files "")
    file(GLOB_RECURSE kernel_core_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/system/kernel/*.cppm")
    list(FILTER kernel_core_files EXCLUDE REGEX "_export\\.cppm$")

    set(kernel_out_import "^[ \t]*(export[ \t]+)?import[ \t]+out\\.[A-Za-z0-9_]")
    charm_check_imports(
        "${kernel_core_files}"
        "${kernel_out_import}"
        "Kernel core module imported out.*; move presentation logic to *_export.cppm")
endfunction()
