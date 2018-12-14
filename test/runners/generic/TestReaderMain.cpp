/*
* Copyright 2016 Nu-book Inc.
* Copyright 2017 Axel Waggershauser
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "BlackboxTestRunner.h"
#include "ImageLoader.h"
#include "TestReader.h"
#include "GenericLuminanceSource.h"
#include "HybridBinarizer.h"
#include "TextUtfEncoding.h"
#include "ZXContainerAlgorithms.h"
#include "lodepng.h"

#include <iostream>
#include <set>

#if defined(__cpp_lib_filesystem)
#   define FILESYSTEM_IS_EXPERIMENTAL 0
#elif defined(__cpp_lib_experimental_filesystem)
#   define FILESYSTEM_IS_EXPERIMENTAL 1
#elif __has_include(<filesystem>)
#   define FILESYSTEM_IS_EXPERIMENTAL 0
#elif __has_include(<experimental/filesystem>)
#   define FILESYSTEM_IS_EXPERIMENTAL 1
#elif defined(__APPLE__) && defined(__GNUC__) && !defined(__clang__)
#   define FILESYSTEM_IS_EXPERIMENTAL 1
#elif __has_include(<dirent.h>)
#   define ZXING_HAVE_DIRENT_H 1
#else
#   error <filesystem> feature is required
#endif

#if !defined(ZXING_HAVE_DIRENT_H)
#    define ZXING_HAVE_DIRENT_H 0
#endif

#if ZXING_HAVE_DIRENT_H
	#include <dirent.h>
#elif FILESYSTEM_IS_EXPERIMENTAL
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#else
    #include <filesystem>
    namespace fs = std::filesystem;
#endif

using namespace ZXing;
using namespace ZXing::Test;

static bool HasSuffix(const std::string& s, const std::string& suffix)
{
	return s.size() >= suffix.size() && std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

class GenericImageLoader : public ImageLoader
{
public:
	virtual std::shared_ptr<LuminanceSource> load(const std::wstring& filename) const override
	{
		std::vector<unsigned char> buffer;
		unsigned width, height;
		unsigned error = lodepng::decode(buffer, width, height, TextUtfEncoding::ToUtf8(filename));
		if (error) {
			throw std::runtime_error("Failed to read image");
		}
		return std::make_shared<GenericLuminanceSource>((int)width, (int)height, buffer.data(), width*4, 4, 0, 1, 2);
	}
};

class GenericBlackboxTestRunner : public BlackboxTestRunner
{
public:
	GenericBlackboxTestRunner(const std::wstring& pathPrefix)
		: BlackboxTestRunner(pathPrefix, std::make_shared<GenericImageLoader>())
	{
	}

	virtual std::vector<std::wstring> getImagesInDirectory(const std::wstring& dirPath) override
	{
		std::vector<std::wstring> result;
#if ZXING_HAVE_DIRENT_H
		if (auto dir = opendir(TextUtfEncoding::ToUtf8(pathPrefix() + L"/" + dirPath).c_str())) {
			while (auto entry = readdir(dir)) {
				if (HasSuffix(entry->d_name, ".png")) {
					result.push_back(dirPath + L"/" + TextUtfEncoding::FromUtf8(entry->d_name));
				}
			}
			closedir(dir);
		}
		else {
			std::cerr << "Error open dir " << TextUtfEncoding::ToUtf8(dirPath) << std::endl;
		}
#else
		for (const auto& entry : fs::directory_iterator(fs::path(pathPrefix()) / dirPath))
			if (fs::is_regular_file(entry.status()) && entry.path().extension() == ".png")
				result.push_back(dirPath + L"/" + entry.path().filename().generic_wstring());
#endif
		return result;
	}
};

int main(int argc, char** argv)
{
	if (argc <= 1) {
		std::cout << "Usage: " << argv[0] << " <test_path_prefix>" << std::endl;
		return 0;
	}

	std::string pathPrefix = argv[1];

	GenericBlackboxTestRunner runner(TextUtfEncoding::FromUtf8(pathPrefix));

	if (HasSuffix(pathPrefix, ".png")) {
#if 0
		TestReader reader = runner.createReader(false, false, "QR_CODE");
#else
		TestReader reader = runner.createReader(true, true);
#endif
		bool isPure = getenv("IS_PURE");
		int rotation = getenv("ROTATION") ? atoi(getenv("ROTATION")) : 0;

		for (int i = 1; i < argc; ++i) {
			auto result = reader.read(TextUtfEncoding::FromUtf8(argv[i]), rotation, isPure);
			std::cout << argv[i] << ": ";
			if (result)
				std::cout << result.format << ": " << TextUtfEncoding::ToUtf8(result.text) << "\n";
			else
				std::cout << "FAILED\n";
		}
		return 0;
	}

	std::set<std::string> includedTests;
	for (int i = 2; i < argc; ++i) {
		if (std::strlen(argv[i]) > 2 && argv[i][0] == '-' && argv[i][1] == 't') {
			includedTests.insert(argv[i] + 2);
		}
	}

	runner.run(includedTests);
}
