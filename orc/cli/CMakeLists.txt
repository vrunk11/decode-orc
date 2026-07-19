# Section: CLI build configuration
# Defines the orc-cli executable and associated build settings
add_executable(orc-cli 
    orc_cli.cpp
    command_process.cpp
    command_filter.cpp
    command_plugins.cpp
    stage_category.cpp
    markdown_examples.cpp
)

# Section: Architecture enforcement (MVP)
# CLI uses presenter and common layers; prevents direct core linkage
target_link_libraries(orc-cli PRIVATE 
    orc-presenters
    orc-common
)

# Section: Public include paths for CLI
target_include_directories(orc-cli PRIVATE
    ${CMAKE_SOURCE_DIR}/orc/presenters/include
    ${CMAKE_SOURCE_DIR}/orc/public
    ${CMAKE_SOURCE_DIR}/orc/common/include
    ${CMAKE_BINARY_DIR}/generated
)

# Section: Build-time flags for MVP enforcement
target_compile_definitions(orc-cli PRIVATE
    ORC_CLI_BUILD
)

if(APPLE)
    set_target_properties(orc-cli PROPERTIES INSTALL_RPATH "@executable_path/../Frameworks")
elseif(UNIX AND NOT WIN32)
    set_target_properties(orc-cli PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib;\$ORIGIN/../lib/orc-stage-plugins")
endif()

# Section: Install rules
if(APPLE)
    # Embed CLI inside the macOS app bundle so drag-and-drop install contains both tools
    install(TARGETS orc-cli
        RUNTIME DESTINATION "orc-gui.app/Contents/MacOS"
        COMPONENT Runtime
    )
else()
    install(TARGETS orc-cli
        RUNTIME DESTINATION bin
        COMPONENT Runtime
    )
endif()

if(BUILD_UNIT_TESTS)
    add_test(
        NAME CLI.HelpShowsProcessOnlyCommand
        COMMAND $<TARGET_FILE:orc-cli> --help
    )
    set_tests_properties(CLI.HelpShowsProcessOnlyCommand PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "--process"
    )

    add_test(
        NAME CLI.HelpShowsPluginsSubcommand
        COMMAND $<TARGET_FILE:orc-cli> --help
    )
    set_tests_properties(CLI.HelpShowsPluginsSubcommand PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "plugins"
    )

    add_test(
        NAME CLI.HelpShowsSafeCorePluginsOption
        COMMAND $<TARGET_FILE:orc-cli> --help
    )
    set_tests_properties(CLI.HelpShowsSafeCorePluginsOption PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "--safe-core-plugins"
    )

    add_test(
        NAME CLI.PluginsListWithSafeCorePluginsBeforeSubcommandSucceeds
        COMMAND $<TARGET_FILE:orc-cli> plugins --safe-core-plugins list
    )
    set_tests_properties(CLI.PluginsListWithSafeCorePluginsBeforeSubcommandSucceeds PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "Registry path:"
        ENVIRONMENT "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home-safe-before;HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home-safe-before"
    )

    add_test(
        NAME CLI.PluginsListWithSafeCorePluginsAfterSubcommandSucceeds
        COMMAND $<TARGET_FILE:orc-cli> plugins list --safe-core-plugins
    )
    set_tests_properties(CLI.PluginsListWithSafeCorePluginsAfterSubcommandSucceeds PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "Registry path:"
        ENVIRONMENT "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home-safe-after;HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home-safe-after"
    )

    add_test(
        NAME CLI.PluginsListSucceeds
        COMMAND $<TARGET_FILE:orc-cli> plugins list
    )
    set_tests_properties(CLI.PluginsListSucceeds PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "Registry path:"
    )

    add_test(
        NAME CLI.PluginsAddMissingPathFails
        COMMAND $<TARGET_FILE:orc-cli> plugins add
    )
    set_tests_properties(CLI.PluginsAddMissingPathFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.PluginsRemoveMissingIdFails
        COMMAND $<TARGET_FILE:orc-cli> plugins remove
    )
    set_tests_properties(CLI.PluginsRemoveMissingIdFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.PluginsEnableMissingIdFails
        COMMAND $<TARGET_FILE:orc-cli> plugins enable
    )
    set_tests_properties(CLI.PluginsEnableMissingIdFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.PluginsDisableMissingIdFails
        COMMAND $<TARGET_FILE:orc-cli> plugins disable
    )
    set_tests_properties(CLI.PluginsDisableMissingIdFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.PluginsSearchMissingTermFails
        COMMAND $<TARGET_FILE:orc-cli> plugins search
    )
    set_tests_properties(CLI.PluginsSearchMissingTermFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.PluginsInfoMissingIdFails
        COMMAND $<TARGET_FILE:orc-cli> plugins info
    )
    set_tests_properties(CLI.PluginsInfoMissingIdFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.PluginsInstallMissingIdFails
        COMMAND $<TARGET_FILE:orc-cli> plugins install
    )
    set_tests_properties(CLI.PluginsInstallMissingIdFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.PluginsHelpShowsDiscoverySubcommands
        COMMAND $<TARGET_FILE:orc-cli> plugins --help
    )
    set_tests_properties(CLI.PluginsHelpShowsDiscoverySubcommands PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "search"
    )

    add_test(
        NAME CLI.PluginsUnknownSubcommandFails
        COMMAND $<TARGET_FILE:orc-cli> plugins frobnicate
    )
    set_tests_properties(CLI.PluginsUnknownSubcommandFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.RejectsPreviewExportOption
        COMMAND $<TARGET_FILE:orc-cli> --preview-export
    )
    set_tests_properties(CLI.RejectsPreviewExportOption PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.ProcessNoSinksFails
        COMMAND $<TARGET_FILE:orc-cli> ${CMAKE_CURRENT_SOURCE_DIR}/test-data/no-sinks.orcprj --process
    )
    set_tests_properties(CLI.ProcessNoSinksFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    # --- Filtergraph mode (graph-string) -------------------------------------

    add_test(
        NAME CLI.HelpShowsFilterCommand
        COMMAND $<TARGET_FILE:orc-cli> --help
    )
    set_tests_properties(CLI.HelpShowsFilterCommand PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "--filter"
    )

    add_test(
        NAME CLI.HelpShowsAutoDetectNote
        COMMAND $<TARGET_FILE:orc-cli> --help
    )
    set_tests_properties(CLI.HelpShowsAutoDetectNote PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "automatically"
    )

    add_test(
        NAME CLI.FilterUnknownStageFails
        COMMAND $<TARGET_FILE:orc-cli>
                --filter "definitely_not_a_stage, video_sink"
    )
    set_tests_properties(CLI.FilterUnknownStageFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.FilterMalformedGraphFails
        COMMAND $<TARGET_FILE:orc-cli> --filter "src=badparam, sink"
    )
    set_tests_properties(CLI.FilterMalformedGraphFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.FilterRejectsProjectFileCombination
        COMMAND $<TARGET_FILE:orc-cli> project.orcprj --filter "src, sink"
    )
    set_tests_properties(CLI.FilterRejectsProjectFileCombination PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.FilterMissingRequiredParameterFails
        COMMAND $<TARGET_FILE:orc-cli>
                --filter "tbc_source=input_path=x.tbc, CVBSSink"
    )
    set_tests_properties(CLI.FilterMissingRequiredParameterFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    # --- Export options (--export-project / --to-filter) ---------------------

    add_test(
        NAME CLI.HelpShowsExportOptions
        COMMAND $<TARGET_FILE:orc-cli> --help
    )
    set_tests_properties(CLI.HelpShowsExportOptions PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "--export-project"
    )

    add_test(
        NAME CLI.ToFilterPrintsCanonicalGraph
        COMMAND $<TARGET_FILE:orc-cli>
                --input "tbc_source=input_path=x.tbc" --output CVBSSink=output_path=y.composite
                --to-filter
    )
    set_tests_properties(CLI.ToFilterPrintsCanonicalGraph PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "tbc_source"
    )

    add_test(
        NAME CLI.ToFilterAndExportProjectMutuallyExclusive
        COMMAND $<TARGET_FILE:orc-cli>
                --input "tbc_source=input_path=x.tbc" --output CVBSSink=output_path=y.composite
                --to-filter --export-project out.orcprj
    )
    set_tests_properties(CLI.ToFilterAndExportProjectMutuallyExclusive PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.ExportProjectRejectedWithProjectFile
        COMMAND $<TARGET_FILE:orc-cli> project.orcprj --process
                --export-project out.orcprj
    )
    set_tests_properties(CLI.ExportProjectRejectedWithProjectFile PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.ProcessAndToFilterMutuallyExclusive
        COMMAND $<TARGET_FILE:orc-cli> project.orcprj --process --to-filter
    )
    set_tests_properties(CLI.ProcessAndToFilterMutuallyExclusive PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    # --- Input/filters/output triad -------------------------------------------

    add_test(
        NAME CLI.HelpShowsTriadOptions
        COMMAND $<TARGET_FILE:orc-cli> --help
    )
    set_tests_properties(CLI.HelpShowsTriadOptions PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "--input"
    )

    add_test(
        NAME CLI.TriadRejectsCombinationWithFilter
        COMMAND $<TARGET_FILE:orc-cli> --input "src" --filter "src, sink"
    )
    set_tests_properties(CLI.TriadRejectsCombinationWithFilter PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.TriadRejectsProjectFileCombination
        COMMAND $<TARGET_FILE:orc-cli> project.orcprj --input "src" --output "sink"
    )
    set_tests_properties(CLI.TriadRejectsProjectFileCombination PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.TriadUnknownStageFails
        COMMAND $<TARGET_FILE:orc-cli>
                --input "definitely_not_a_stage" --output "video_sink"
    )
    set_tests_properties(CLI.TriadUnknownStageFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.TriadRejectsSinkStageInInput
        COMMAND $<TARGET_FILE:orc-cli> --input "CVBSSink" --output "video_sink"
    )
    set_tests_properties(CLI.TriadRejectsSinkStageInInput PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.TriadRejectsSourceStageInOutput
        COMMAND $<TARGET_FILE:orc-cli>
                --input "tbc_source=input_path=x.tbc" --output "tbc_source=input_path=y.tbc"
    )
    set_tests_properties(CLI.TriadRejectsSourceStageInOutput PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.TriadRejectsSourceStageInFilters
        COMMAND $<TARGET_FILE:orc-cli>
                --input "tbc_source=input_path=x.tbc" --filters "tbc_source=input_path=y.tbc"
                --output "video_sink"
    )
    set_tests_properties(CLI.TriadRejectsSourceStageInFilters PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    # --- Plugin/stage introspection --------------------------------------------

    add_test(
        NAME CLI.PluginsStagesLists
        COMMAND $<TARGET_FILE:orc-cli> plugins stages
    )
    set_tests_properties(CLI.PluginsStagesLists PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "Available stages"
    )

    add_test(
        NAME CLI.PluginsStagesShowsRoleLabel
        COMMAND $<TARGET_FILE:orc-cli> plugins stages
    )
    set_tests_properties(CLI.PluginsStagesShowsRoleLabel PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "input .source."
    )

    add_test(
        NAME CLI.PluginsInputsListsOnlySources
        COMMAND $<TARGET_FILE:orc-cli> plugins inputs
    )
    set_tests_properties(CLI.PluginsInputsListsOnlySources PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "Available input .source."
    )

    add_test(
        NAME CLI.PluginsOutputsListsOnlySinks
        COMMAND $<TARGET_FILE:orc-cli> plugins outputs
    )
    set_tests_properties(CLI.PluginsOutputsListsOnlySinks PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "Available output .sink."
    )

    add_test(
        NAME CLI.PluginsFiltersListsOnlyProcessing
        COMMAND $<TARGET_FILE:orc-cli> plugins filters
    )
    set_tests_properties(CLI.PluginsFiltersListsOnlyProcessing PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "Available processing"
    )

    add_test(
        NAME CLI.PluginsStagesFullShowsParameters
        COMMAND $<TARGET_FILE:orc-cli> plugins stages --full
    )
    set_tests_properties(CLI.PluginsStagesFullShowsParameters PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "required|optional|no parameters"
    )

    add_test(
        NAME CLI.PluginsDescribeUnknownStageFails
        COMMAND $<TARGET_FILE:orc-cli> plugins describe definitely_not_a_stage
    )
    set_tests_properties(CLI.PluginsDescribeUnknownStageFails PROPERTIES
        LABELS "unit;cli"
        WILL_FAIL TRUE
    )

    add_test(
        NAME CLI.PluginsDescribeShowsExamplesSection
        COMMAND $<TARGET_FILE:orc-cli> plugins describe video_sink
    )
    set_tests_properties(CLI.PluginsDescribeShowsExamplesSection PROPERTIES
        LABELS "unit;cli"
        PASS_REGULAR_EXPRESSION "Examples:"
    )

    add_test(
        NAME CLI.PluginsDescribeExamplesNotDuplicated
        COMMAND $<TARGET_FILE:orc-cli> plugins describe video_sink
    )
    # The raw instructions text and the specially-formatted recap must not
    # both print the "## Examples" heading literally: only the recap's own
    # "Examples:" label should appear, not the Markdown heading text.
    set_tests_properties(CLI.PluginsDescribeExamplesNotDuplicated PROPERTIES
        LABELS "unit;cli"
        FAIL_REGULAR_EXPRESSION "## Examples"
    )

    # Pure unit test for the filtergraph parser (no orc-core dependency).
    find_package(GTest REQUIRED)
    # filtergraph_parser now lives in orc/presenters (so orc-gui can share it
    # too), but this test intentionally compiles it directly rather than
    # linking orc-presenters, to keep it a fast, fully standalone test with
    # no orc-core dependency.
    add_executable(orc-cli-filtergraph-parser-test
        filtergraph_parser_test.cpp
        ${CMAKE_SOURCE_DIR}/orc/presenters/src/filtergraph_parser.cpp
    )
    target_include_directories(orc-cli-filtergraph-parser-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/orc/presenters/include
    )
    target_link_libraries(orc-cli-filtergraph-parser-test PRIVATE
        GTest::gtest_main
    )
    add_test(
        NAME CLI.FiltergraphParserUnit
        COMMAND $<TARGET_FILE:orc-cli-filtergraph-parser-test>
    )
    set_tests_properties(CLI.FiltergraphParserUnit PROPERTIES
        LABELS "unit;cli"
    )

    # Pure unit test for the Markdown "## Examples" extractor (no orc-core
    # dependency).
    add_executable(orc-cli-markdown-examples-test
        markdown_examples_test.cpp
        markdown_examples.cpp
    )
    target_include_directories(orc-cli-markdown-examples-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(orc-cli-markdown-examples-test PRIVATE
        GTest::gtest_main
    )
    add_test(
        NAME CLI.MarkdownExamplesUnit
        COMMAND $<TARGET_FILE:orc-cli-markdown-examples-test>
    )
    set_tests_properties(CLI.MarkdownExamplesUnit PROPERTIES
        LABELS "unit;cli"
    )
endif()
