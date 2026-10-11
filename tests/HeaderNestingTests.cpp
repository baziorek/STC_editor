// The tree of headers of the conspect: which header is the parent of which. Only the algorithm, no widget.

#include <gtest/gtest.h>

#include "utils/HeaderNesting.h"

TEST(HeaderNestingTest, SectionsSitUnderTheClosestHeaderAboveWithSmallerLevel)
{
    // h1, h2, h3, h2, h1
    const auto parents = headerParents({1, 2, 3, 2, 1});
    EXPECT_EQ(parents, (std::vector<int>{-1, 0, 1, 0, -1}));
}

TEST(HeaderNestingTest, SkippedLevelsAttachToTheClosestShallowerHeader)
{
    // h1, h4, h2 : the h4 has no h2/h3 above it, it is still under h1; the h2 is under h1 too
    const auto parents = headerParents({1, 4, 2});
    EXPECT_EQ(parents, (std::vector<int>{-1, 0, 0}));
}

TEST(HeaderNestingTest, HeadersWithoutAShallowerOneAreTopLevel)
{
    EXPECT_EQ(headerParents({2, 2, 1}), (std::vector<int>{-1, -1, -1}));
    EXPECT_TRUE(headerParents({}).empty());
}
