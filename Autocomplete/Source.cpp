#include "Automata.h"

int main(int, char *[]) {
	std::string files[] = {
		"lists/1k.txt", "lists/3k.txt", "lists/58k.txt", "lists/naughty.txt"
	};

	std::string &fileName = files[0];

	Automata dict;
	{
		std::ifstream wordFile(fileName);
		if (!wordFile) {
			std::cout << "Can't open file";
			return 0;
		}

		std::cout << "Reading ..." << std::endl;
		std::string line;
		std::vector<std::string> words;
		while (getline(wordFile, line)) {
			while (line.back() == '\r' || line.back() == '\n') {
				line.pop_back();
			}
			words.push_back(line);
		}

		std::cout << "Building ..." << std::endl;
		std::vector<std::string> testWords = {
			"follow", "feast", "fear", "fart", "farting", "pestering", "pester"
			// "test", "tests", "testing", "tester", "teaser", "training", "pining"
		};
		dict.buildFromWordList(testWords.empty() ? words : testWords);

		std::cout << "Writing grapviz ..." << std::endl;
		dict.dumpGraph(dict.getDefaultGraphDump("viz.dot"));

		std::cout << "Running tests ..." << std::endl;
		ac_assert(dict.runVerify());
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