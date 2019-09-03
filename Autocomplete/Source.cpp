#include "Automata.h"

#include <chrono>
#include <sstream>
#include <memory>


typedef std::shared_ptr<std::istream> FilePtr;
struct FileWithPath {
	std::string path;
	FilePtr file;
	FileWithPath(std::string str, FilePtr file)
		: path(std::move(str))
		, file(std::move(file))
	{}
};

bool readFileLines(const FileWithPath &file, Automata::WordList &list) {
	list.clear();
	if (!file.file) {
		std::cerr << "Failed to read from " << file.path << std::endl;
		return false;
	}
	file.file->seekg(0);

	std::cout << "Reading ..." << std::endl;
	std::string line;
	while (getline(*file.file, line)) {
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
			line.pop_back();
		}
		list.push_back(line);
	}

	return true;
}

struct timer {
	typedef std::chrono::high_resolution_clock clock_t;
	typedef std::chrono::high_resolution_clock::time_point time_point;
	typedef std::chrono::milliseconds ms_t;

	std::string name;
	time_point start;
	bool print = true;

	timer(const std::string &name)
		: name(name)
		, start(clock_t::now())
	{}

	ms_t::rep getElapsed() {
		const ms_t elapsed = std::chrono::duration_cast<ms_t>(clock_t::now() - start);
		print = false;
		return elapsed.count();
	}

	~timer() {
		if (print) {
			const ms_t::rep ms = getElapsed();
			std::cout << "Timer : [" << name << "] " << ms << "ms." << std::endl;
		}
	}
};


int main(int argc, char *argv[]) {
	std::vector<std::string> filePaths = {
		"lists/1k.txt",
		"lists/3k.txt",
		"lists/58k.txt",
#if !AC_ASSERT_ENABLED
		// too slow for debug + assert
		"lists/370k.txt",
#endif
		"lists/naughty.txt"
	};

	bool timeTest = true; // set to true to force time test
	std::string overrideFile;

	if (argc > 1) {
		for (int c = 1; c < argc; c++) {
			const char *param = argv[c];
			const bool hasMore = c + 1 < argc;
			const char *next = hasMore ? argv[c + 1] : nullptr;
			if (!strcmp(param, "--time")) {
				timeTest = true;
			} else if (!strcmp(param, "--file") && next) {
				filePaths.clear();
				filePaths.emplace_back(next);
			}
		}
	}

	std::vector<FileWithPath> files;

	if (filePaths.size() > 1) {
		const std::vector<std::string> testWords = {
			"follow", "feast", "fear", "fart", "farting", "pestering", "pester", "testtest",
			"test", "tests", "testing", "tester", "teaser", "training", "pining", "test", "te",
			"aAZ", "bAB", "eAB", "eAZ",
		};
		std::unique_ptr<std::stringstream> fakeFile(new std::stringstream);
		for (const std::string &word : testWords) {
			(*fakeFile) << word << std::endl;
		}
		files.emplace_back("$fakeFile", std::move(fakeFile));
	}

	for (const std::string &fpath : filePaths) {
		files.emplace_back(fpath, FilePtr(new std::ifstream(fpath)));
	}

	if (timeTest) {
		/*
		 * lists/1k.txt states: 964 / 1
		 * lists/3k.txt states: 2621 / 4.2
		 * lists/58k.txt states: 27025 / 92.4
		 * lists/370k.txt states: 160306 / 727.6
		 * lists/naughty.txt states: 925 / 0
		 */
		int64_t collisions = 0;
		for (const FileWithPath &pair : files) {
			Automata::WordList words;
			if (!readFileLines(pair, words)) {
				continue;
			}
#if AC_ASSERT_ENABLED
			Automata dict;
			std::cout << "Building..." << std::endl;
			dict.buildFromWordList(words);
			std::cout << "Verify: " << dict.runVerify() << std::endl;
			std::cout << pair.path << " states: " << dict.getNumberOfStates() << std::endl;
			std::cout << "Running tests ..." << std::endl;
			ac_assert(dict.runVerify());
#else
			timer::ms_t::rep total = 0;
			const int repeat = 25;
			int states = 0;
			for (int c = 0; c < repeat; c++) {
				Automata dict;
				{
					timer t("");
					dict.buildFromWordList(words);
					total += t.getElapsed();
				}
				collisions += dict.getBuildCollisions();
				states = dict.getNumberOfStates();
			}
			std::cout << pair.path << " states " << states << std::endl;
			std::cout << "Time for " << pair.path << ": " << (total / double(repeat)) << "ms." << std::endl;
#endif
		}
		std::cout << "Collisions: " << collisions << std::endl;

		return 0;
	}

	const FileWithPath &file = files[0];
	Automata dict;
	{
		Automata::WordList words;
		if (!readFileLines(file, words)) {
			return 0;
		}

		std::cout << "Building ..." << std::endl;
		dict.buildFromWordList(std::move(words));

		std::cout << "Writing graph-viz ..." << std::endl;
		dict.dumpGraph(dict.getDefaultGraphDump("viz.dot"));

		std::cout << "Running tests ..." << std::endl;
		ac_assert(dict.runVerify());

		std::cout << "States in automata: " << dict.getNumberOfStates() << std::endl;
		std::cout << "Words in automata: " << dict.getNumberOfWords() << std::endl;
		std::cout << "Symbols in automata: " << dict.getNumberOfTotalSymbols() << std::endl;
	}

	std::string input;

	std::cout << "Enter prefix: ";
	while (std::cin >> input) {
		std::set<std::string> suffixes;
		dict.getSuffixes(input, suffixes);

		if (suffixes.empty()) {
			std::cout << "> no suffixes" << std::endl;
		} else {
			for (const std::string &suffix : suffixes) {
				std::cout << input << suffix << std::endl;
			}
			std::cout << "> " << suffixes.size() << " suffixes" << std::endl;
		}
	}

	return 0;
}