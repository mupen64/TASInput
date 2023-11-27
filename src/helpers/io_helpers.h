#pragma once

#include <filesystem>
#include <Windows.h>
#include <string>
#include <vector>

/**
 * \brief Gets all files with a specific file extension directly under a directory
 * \param directory The path joiner-terminated directory
 * \param extension The file extension with no period
 */
std::vector<std::string> get_files_with_extension_in_directory(
	const std::string &directory, const std::string &extension);
/**
 * \brief Gets all files under all subdirectory of a specific directory, including the directory's shallow files
 * \param directory The path joiner-terminated directory
 */
std::vector<std::string> get_files_in_subdirectories(
	const std::string& directory);

/**
 * \brief Removes the extension from a path
 * \param path The path to remove the extension from
 * \return The path without an extension
 */
std::string strip_extension(const std::string& path);

/**
 * \brief Removes the extension from a path
 * \param path The path to remove the extension from
 * \return The path without an extension
 */
std::wstring strip_extension(const std::wstring &path);

/**
 * \brief Copies a string to the clipboard
 * \param owner The clipboard content's owner window
 * \param str The string to be copied
 */
void copy_to_clipboard(HWND owner, const std::string &str);

/**
 * \brief Gets the path to the current user's desktop
 */
std::wstring get_desktop_path();

/**
 * \brief Gets whether a file is accessible
 * \param path The file to check against
 */
bool is_file_accessible(const std::filesystem::path& path);

/**
 * \brief Writes data to a vector at its tail end based on its size, expanding it as needed
 * \param vec The target vector
 * \param data The source data
 * \param len The source data's size in bytes
 */
void vecwrite(std::vector<uint8_t>& vec, void* data, size_t len);

/**
 * \brief Reads a file into a buffer
 * \param path The file's path
 * \return The file's contents, or an empty vector if the operation failed
 */
std::vector<uint8_t> read_file_buffer(const std::filesystem::path& path);

/**
 * \brief Reads source data into the destination, advancing the source pointer by <c>len</c>
 * \param src A pointer to the source data
 * \param dest The destination buffer
 * \param len The destination buffer's length
 */
void memread(uint8_t** src, void* dest, unsigned int len);
