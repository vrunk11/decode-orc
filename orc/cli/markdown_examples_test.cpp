/*
 * File:        markdown_examples_test.cpp
 * Module:      orc-cli-tests
 * Purpose:     Unit tests for the "## Examples" Markdown section extractor.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#include "markdown_examples.h"

#include <gtest/gtest.h>

namespace {

using orc::cli::extract_examples_section;
using orc::cli::remove_examples_section;

TEST(MarkdownExamples, BasicExampleWithDescription) {
  const std::string md = R"(# TBC Source

Some intro text.

## When to use

Blah.

## Examples

Decode a composite capture to MP4:

```bash
orc-cli --input "tbc_source=input_path=capture.tbc" --output video_sink
```

## Parameters

Not part of examples.
)";
  auto r = extract_examples_section(md);
  ASSERT_TRUE(r.found);
  ASSERT_EQ(r.examples.size(), 1u);
  EXPECT_EQ(r.examples[0].description, "Decode a composite capture to MP4:");
  EXPECT_EQ(r.examples[0].command,
            "orc-cli --input \"tbc_source=input_path=capture.tbc\" --output "
            "video_sink");
}

TEST(MarkdownExamples, NoExamplesSectionAtAll) {
  const std::string md = "# Foo\n\n## Parameters\n\nNone.\n";
  auto r = extract_examples_section(md);
  EXPECT_FALSE(r.found);
  EXPECT_TRUE(r.examples.empty());
}

TEST(MarkdownExamples, ExamplesSectionWithNoFences) {
  const std::string md = "## Examples\n\nJust prose, no code.\n\n## Parameters\n";
  auto r = extract_examples_section(md);
  EXPECT_TRUE(r.found);
  EXPECT_TRUE(r.examples.empty());
}

TEST(MarkdownExamples, MultipleExamples) {
  const std::string md = R"(## Examples

First one:

```bash
cmd1
```

Second one:

```bash
cmd2
```
)";
  auto r = extract_examples_section(md);
  ASSERT_TRUE(r.found);
  ASSERT_EQ(r.examples.size(), 2u);
  EXPECT_EQ(r.examples[0].description, "First one:");
  EXPECT_EQ(r.examples[0].command, "cmd1");
  EXPECT_EQ(r.examples[1].description, "Second one:");
  EXPECT_EQ(r.examples[1].command, "cmd2");
}

TEST(MarkdownExamples, MultiLineCommandPreservesLineBreaks) {
  const std::string md = R"(## Examples

Multi-line:

```bash
orc-cli --input "a" \
  --output "b"
```
)";
  auto r = extract_examples_section(md);
  ASSERT_TRUE(r.found);
  ASSERT_EQ(r.examples.size(), 1u);
  EXPECT_EQ(r.examples[0].command, "orc-cli --input \"a\" \\\n  --output \"b\"");
}

TEST(MarkdownExamples, FenceWithNoPrecedingDescription) {
  const std::string md = "## Examples\n\n```bash\nbarecmd\n```\n";
  auto r = extract_examples_section(md);
  ASSERT_TRUE(r.found);
  ASSERT_EQ(r.examples.size(), 1u);
  EXPECT_TRUE(r.examples[0].description.empty());
}

TEST(MarkdownExamples, Level3HeadingStaysInSectionButLevel2Ends) {
  const std::string md = R"(## Examples

### Basic usage

desc1

```bash
cmd1
```

## Parameters

not included

```bash
should_not_appear
```
)";
  auto r = extract_examples_section(md);
  ASSERT_TRUE(r.found);
  EXPECT_EQ(r.examples.size(), 1u);
}

TEST(MarkdownExamples, LowercaseHeadingDoesNotMatch) {
  const std::string md = "## examples\n\n```bash\ncmd\n```\n";
  auto r = extract_examples_section(md);
  EXPECT_FALSE(r.found);
}

TEST(MarkdownExamples, TrailingHashAtxHeadingRecognised) {
  const std::string md = "## Examples ##\n\n```bash\ncmd\n```\n";
  auto r = extract_examples_section(md);
  EXPECT_TRUE(r.found);
}

TEST(MarkdownExamples, EmptyDocument) {
  auto r = extract_examples_section("");
  EXPECT_FALSE(r.found);
}

TEST(MarkdownExamples, UnterminatedFenceYieldsNoExample) {
  const std::string md = "## Examples\n\ndesc\n\n```bash\ncmd_never_closed\n";
  auto r = extract_examples_section(md);
  EXPECT_TRUE(r.found);
  EXPECT_TRUE(r.examples.empty());
}

TEST(MarkdownExamplesRemoval, RemovesHeadingAndBodyOnly) {
  const std::string md =
      "# Foo\n\n## Examples\n\ntext\n\n```bash\ncmd\n```\n\n## Parameters\n\n"
      "some params\n";
  const std::string out = remove_examples_section(md);
  EXPECT_EQ(out.find("## Examples"), std::string::npos);
  EXPECT_EQ(out.find("cmd"), std::string::npos);
  EXPECT_NE(out.find("## Parameters"), std::string::npos);
  EXPECT_NE(out.find("some params"), std::string::npos);
  EXPECT_NE(out.find("# Foo"), std::string::npos);
}

TEST(MarkdownExamplesRemoval, NoOpWhenNoExamplesSection) {
  const std::string md = "# Foo\n\n## Parameters\n\nNone.\n";
  EXPECT_EQ(remove_examples_section(md), md);
}

TEST(MarkdownExamplesRemoval, HandlesSectionAtEndOfDocument) {
  const std::string md =
      "# Foo\n\nintro\n\n## Examples\n\n```bash\ncmd\n```\n";
  const std::string out = remove_examples_section(md);
  EXPECT_EQ(out.find("## Examples"), std::string::npos);
  EXPECT_EQ(out.find("cmd"), std::string::npos);
  EXPECT_NE(out.find("intro"), std::string::npos);
}

}  // namespace
