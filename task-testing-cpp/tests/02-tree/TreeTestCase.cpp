//
// Created by akhtyamovpavel on 5/1/20.
//


#include "TreeTestCase.h"
#include "Tree.h"

TEST_F(FileFixture, IGOTNOBRAINS) {
    std::filesystem::path file = temp_dir / "test.txt";
    std::ofstream(file) << "Fixture test";
    ASSERT_TRUE(std::filesystem::file_size(file) > 0);

    EXPECT_THROW(GetTree("LEOPARD2A4/PZB/123", false), std::invalid_argument);
    EXPECT_ANY_THROW(GetTree(file.string(), false));
}

TEST_F(FileFixture, EmptyDirectory) {
    FileNode result = GetTree(temp_dir.string(), false);
    
    EXPECT_EQ(result.name, temp_dir.filename());
    EXPECT_TRUE(result.is_dir);
    EXPECT_TRUE(result.children.empty());
}

TEST_F(FileFixture, MixedContent) {    
    std::filesystem::create_directories(temp_dir / "subdir");
    std::ofstream(temp_dir / "file1.txt");
    std::ofstream(temp_dir / "subdir/file2.txt");

    // Тест без dirs_only
    FileNode result = GetTree(temp_dir.string(), false);
    
    EXPECT_EQ(result.name, temp_dir.filename());
    ASSERT_EQ(result.children.size(), 2);
    
    int index = 1;
    if (result.children[0].is_dir) {
        index = 0;
    }
    EXPECT_EQ(result.children[index].name, "subdir");
    EXPECT_TRUE(result.children[index].is_dir);
    ASSERT_EQ(result.children[index].children.size(), 1);
    
    // Проверяем файл
    EXPECT_EQ(result.children[int(!bool(index))].name, "file1.txt");
    EXPECT_FALSE(result.children[int(!bool(index))].is_dir);
    
    // Тест с dirs_only = true
    FileNode dirs_only_result = GetTree(temp_dir.string(), true);
    ASSERT_EQ(dirs_only_result.children.size(), 1); // Только subdir
}

TEST(FileNodeTest, EqualityOperator) {
    // Одинаковые узлы
    FileNode node1{"dir", true, {}};
    FileNode node2{"dir", true, {}};
    EXPECT_TRUE(node1 == node2);

    // Разные имена
    FileNode node3{"file.txt", false, {}};
    EXPECT_FALSE(node1 == node3);

    // Разный тип
    FileNode node4{"dir", false, {}};
    EXPECT_FALSE(node1 == node4);

    // Разные дети
    FileNode node5{"dir", true, {FileNode{"child", true, {}}}};
    EXPECT_FALSE(node1 == node5);
}

TEST_F(FilterEmptyNodesTest, RemovesEmptyLeafDirectories) {
    // Исходное дерево
    FileNode root = GetTree("test_root", true);
    
    // Применяем фильтр
    FilterEmptyNodes(root, "test_root");
    
    EXPECT_FALSE(std::filesystem::exists("test_root/empty_dir"));
    
    EXPECT_TRUE(std::filesystem::exists("test_root/a"));
    EXPECT_TRUE(std::filesystem::exists("test_root/file1.txt"));
}

TEST_F(FilterEmptyNodesTest, ThrowsWhenTryingToRemoveRoot) {
    FileNode root;
    root.name = ".";
    root.is_dir = true;
    
    EXPECT_THROW(FilterEmptyNodes(root), std::runtime_error);
}

TEST_F(FilterEmptyNodesTest, RecursivelyRemovesNestedEmptyDirs) {
    // Удаляем все файлы
    std::filesystem::remove("test_root/a/file2.txt");
    
    FileNode root = GetTree("test_root", false);
    FilterEmptyNodes(root, "test_root");
    
    // Проверяем рекурсивное удаление
    EXPECT_FALSE(std::filesystem::exists("test_root/a/b/c"));
}

TEST(ONEMORE, NonDirInput) {
    FileNode root {.name="test", .is_dir=false};
    EXPECT_NO_THROW (FilterEmptyNodes(root));
}
