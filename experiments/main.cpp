#include "duckdb.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <cerrno>
#include <stdexcept>
#include <utility>
#include <cctype>

// #define DEBUG

// NOTE: if file has multiple select queries, it seems result->Print() only prints the result from first query

using namespace duckdb;

struct stream_stats {
	double throughput;
	std::vector<std::pair<int, double>> latencies;
};

std::string readFile(const std::string& path);
std::string preprocessTpchQuery(const std::string& input, bool *modified);
bool createDirectory(const std::string& path);
std::string getArgumentValue(const std::string& arg, const std::string& prefix);
std::string generateTimeStamp();

std::vector<double> individualTPCH(Connection &con, std::string dataDir, std::string queryDir, bool verbose);
stream_stats streamTPCH(Connection &con, std::string dataDir, std::string queryDir, int rounds, bool verbose, const std::vector<int>& queryList);
double joinMicrobenchmark(Connection &con, std::string dataDir, bool verbose);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		std::cerr << "Check readme for usage instructions" << std::endl;
		return 1;
	}
	std::string experiment = argv[1];

	DuckDB db(nullptr);
	Connection con(db);

	try {
		if (experiment == "tpch_individual") {
			std::string dataDir, logDir, logData, logName, queryDir;

			if (argc < 6) {
				std::cerr << "Check readme for usage instructions, incorrect number of arguments" << std::endl;
				return 1;
			}

			for (int i = 2; i < argc; ++i) {
				std::string arg = argv[i];
				if (arg.find("-dataDir=") == 0) {
					dataDir = getArgumentValue(arg, "-dataDir=");
				} else if (arg.find("-logDir=") == 0) {
					logDir = getArgumentValue(arg, "-logDir=");
				} else if (arg.find("-logData=") == 0) {
					logData = getArgumentValue(arg, "-logData=");
				} else if (arg.find("-logName=") == 0) {
					logName = getArgumentValue(arg, "-logName=");
				} else if (arg.find("-queryDir=") == 0) {
					queryDir = getArgumentValue(arg, "-queryDir=");
				}
			}
 
			std::vector<double> timings = individualTPCH(con, dataDir, queryDir, false);
			std::string logDirPath = "./results/" + logDir + "/";
			createDirectory(logDirPath);

			std::string logFilePath = logDirPath + logName + ".txt";
			std::ofstream logFile(logFilePath);
			if (!logFile) {
				std::cerr << "Error: Could not open file " << logFilePath << " for writing." << std::endl;
				return 1;
			}

			logFile << "TPC-H Individual Query Experiment" << std::endl;
			logFile << "Dataset in: " << dataDir << std::endl;
			logFile << "Extra details: " << logData << std::endl;
			for (int i = 1; i <= 22; ++i) {
				logFile << "Query " << i << ": " << timings[i-1] << " milliseconds" << std::endl;
			}
		} else if (experiment == "tpch_stream") {
			std::string dataDir, logDir, logData, logName, queryDir;
			int rounds = 2;
			std::vector<int> queryList;

			if (argc < 6) {
				std::cerr << "Check readme for usage instructions, incorrect number of arguments" << std::endl;
				return 1;
			}

			for (int i = 2; i < argc; ++i) {
				std::string arg = argv[i];
				if (arg.find("-dataDir=") == 0) {
					dataDir = getArgumentValue(arg, "-dataDir=");
				} else if (arg.find("-logDir=") == 0) {
					logDir = getArgumentValue(arg, "-logDir=");
				} else if (arg.find("-logData=") == 0) {
					logData = getArgumentValue(arg, "-logData=");
				} else if (arg.find("-Rounds=") == 0) {
					rounds = std::stoi(getArgumentValue(arg, "-Rounds="));
				} else if (arg.find("-logName=") == 0) {
					logName = getArgumentValue(arg, "-logName=");
				} else if (arg.find("-queryDir=") == 0) {
					queryDir = getArgumentValue(arg, "-queryDir=");
				} else if (arg.find("-Queries=") == 0) {
					std::string queryStr = getArgumentValue(arg, "-Queries=");
					std::stringstream ss(queryStr);
					std::string item;
					while (std::getline(ss, item, ',')) {
						queryList.push_back(std::stoi(item));
					}
				}
			}

			stream_stats ss = streamTPCH(con, dataDir, queryDir, rounds, true, queryList);
			std::string logDirPath = "./results/" + logDir + "/";
			createDirectory(logDirPath);

			std::string logFilePath = logDirPath + logName + ".txt";

			std::ofstream logFile(logFilePath);
			if (!logFile) {
				std::cerr << "Error: Could not open file " << logFilePath << " for writing." << std::endl;
				return 1;
			}

			logFile << "TPC-H Stream Experiment" << std::endl;
			logFile << "Dataset in: " << dataDir << std::endl;
			logFile << "Extra details: " << logData << std::endl;
			logFile << "Throughput (QPS): " << ss.throughput << std::endl;
			for (std::pair<int, double>& p : ss.latencies) {
				logFile << "Query " << p.first << " Latency: " << p.second << " milliseconds" << std::endl;
			}
		} else if (experiment == "join_microbench") {
			std::string dataDir, logDir, logData, logName;

			if (argc < 5) {
				std::cerr << "Incorrect number of arguments" << std::endl;
				return 1;
			}

			for (int i = 2; i < argc; ++i) {
				std::string arg = argv[i];
				if (arg.find("-dataDir=") == 0) {
					dataDir = getArgumentValue(arg, "-dataDir=");
				} else if (arg.find("-logDir=") == 0) {
					logDir = getArgumentValue(arg, "-logDir=");
				} else if (arg.find("-logData=") == 0) {
					logData = getArgumentValue(arg, "-logData=");
				} else if (arg.find("-logName=") == 0) {
					logName = getArgumentValue(arg, "-logName=");
				}
			}
 
			double timeTaken = joinMicrobenchmark(con, dataDir, false);
			std::string logDirPath = "./results/" + logDir + "/";
			createDirectory(logDirPath);

			std::string logFilePath = logDirPath + logName + ".txt";
			std::ofstream logFile(logFilePath);
			if (!logFile) {
				std::cerr << "Error: Could not open file " << logFilePath << " for writing." << std::endl;
				return 1;
			}

#ifdef DEBUG
		std::cout << "Logging time taken: " << timeTaken << " milliseconds" << std::endl;
#endif

			logFile << "Join Microbenchmark" << std::endl;
			logFile << "Dataset in: " << dataDir << std::endl;
			logFile << "Extra details: " << logData << std::endl;
			logFile << "Time Taken: " << timeTaken << " milliseconds" << std::endl;
		} else {
			std::cerr << "Unknown experiment specifier" << std::endl;
			return 1;
		}
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}

std::string getArgumentValue(const std::string& arg, const std::string& prefix) {
	if (arg.find(prefix) == 0) {
		return arg.substr(prefix.length());
	}
	return "";
}

bool createDirectory(const std::string& path) {
	int status = mkdir(path.c_str(), 0755);

	if (status == 0) {
		return true;
	} else {
		if (errno == EEXIST) {
			return true;
		} else {
			std::cerr << "Failed to create directory: " << path << std::endl;
			return false;
		}
	}
}


std::string generateTimeStamp() {
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);

	std::stringstream ss;
	std::tm localTime = *std::localtime(&time_t_now);

	char buffer[80];
	std::strftime(buffer, sizeof(buffer), "%H_%M_%S", &localTime);

	ss << buffer;
	return ss.str();
}

std::string readFile(const std::string& path) {
	std::ifstream fileToRead(path);
	if (!fileToRead) {
		throw std::runtime_error("Could not open file: " + path);
	}
	std::stringstream buf;
	buf << fileToRead.rdbuf();
	std::string contents = buf.str();
	if (contents.empty()) {
		throw std::runtime_error("File is empty: " + path);
	}
	return contents;
}

std::string preprocessTpchQuery(const std::string& input, bool *modified) {
	if (modified) {
		*modified = false;
	}
	std::stringstream in(input);
	std::string line;
	std::ostringstream out;
	while (std::getline(in, line)) {
		size_t pos = line.find_first_not_of(" \t\r");
		if (pos != std::string::npos && line[pos] == ':') {
			if (modified) {
				*modified = true;
			}
			continue;
		}
		std::string processed;
		processed.reserve(line.size());
		for (size_t i = 0; i < line.size(); ) {
			if (line[i] == ':' && i + 1 < line.size()) {
				unsigned char next = static_cast<unsigned char>(line[i + 1]);
				if (std::isdigit(next)) {
					if (modified) {
						*modified = true;
					}
					size_t j = i + 1;
					while (j < line.size() &&
						std::isdigit(static_cast<unsigned char>(line[j]))) {
						j++;
					}
					processed += "1";
					i = j;
					continue;
				}
				if (std::isalpha(next) || line[i + 1] == '_') {
					if (modified) {
						*modified = true;
					}
					size_t j = i + 1;
					while (j < line.size()) {
						unsigned char ch = static_cast<unsigned char>(line[j]);
						if (!std::isalnum(ch) && line[j] != '_') {
							break;
						}
						j++;
					}
					std::string token = line.substr(i + 1, j - (i + 1));
					processed += "_" + token;
					i = j;
					continue;
				}
			}
			processed.push_back(line[i]);
			i++;
		}
		out << processed << "\n";
	}
	return out.str();
}

double joinMicrobenchmark(Connection &con, std::string dataDir, bool verbose) {
	double totalTime;
	std::string query;

	std::cout << "\n>>> BEGIN Join Microbenchmark:\n" << std::endl;

	std::cout << "\n>>> INIT SCHEMA... \n" << std::endl;
	query = readFile(dataDir + "schema.sql");
#ifdef DEBUG
	std::cout << query << std::endl;
#endif
	auto result = con.Query(query);
	if (!result || result->HasError()) {
		throw std::runtime_error("Schema load failed: " + (result ? result->GetError() : "null result"));
	}

	std::cout << "\n>>> LOAD DATA... \n" << std::endl;
	query = readFile(dataDir + "load.sql");
#ifdef DEBUG
	std::cout << query << std::endl;
#endif
	result = con.Query(query);
	if (!result || result->HasError()) {
		throw std::runtime_error("Data load failed: " + (result ? result->GetError() : "null result"));
	}

	std::cout << "\n>>> RUN JOIN... " << std::endl;

	query = readFile(dataDir + "query.sql");
#ifdef DEBUG
	std::cout << query << std::endl;
#endif
	auto start = std::chrono::high_resolution_clock::now();
	result = con.Query(query);
	if (!result || result->HasError()) {
		throw std::runtime_error("Query execution failed: " + (result ? result->GetError() : "null result"));
	}
	auto end = std::chrono::high_resolution_clock::now();

	if (verbose) {
		result->Print();
	}

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	if (verbose) {
		std::cout << "JOIN TOOK: " << duration.count() << " milliseconds" << std::endl;
	}

#ifdef DEBUG
	std::cout << "JOIN TOOK: " << duration.count() << " milliseconds" << std::endl;
#endif

	return duration.count();
}

stream_stats streamTPCH(Connection &con, std::string dataDir, std::string queryDir, int rounds, bool verbose, const std::vector<int>& queryList) {
	std::vector<std::pair<int, double>> latencies;
	double totalTime = 0;
	int totalQueries = 0;
	std::vector<std::string> queries;
	std::string query;
	int zero_ms_count = 0;
	bool warned_templates = false;

	std::cout << "\n>>> BEGIN TPC-H Stream Test:\n" << std::endl;

	std::cout << "\n>>> INIT SCHEMA... \n" << std::endl;
	query = readFile(dataDir + "schema.sql");
	auto result = con.Query(query);
	if (!result || result->HasError()) {
		throw std::runtime_error("Schema load failed: " + (result ? result->GetError() : "null result"));
	}

	std::cout << "\n>>> LOAD DATA... \n" << std::endl;
	query = readFile(dataDir + "load.sql");
	result = con.Query(query);
	if (!result || result->HasError()) {
		throw std::runtime_error("Data load failed: " + (result ? result->GetError() : "null result"));
	}

	std::cout << "\n>>> LOAD QUERIES... \n" << std::endl;
	for (int i = 1; i <= 22; ++i) {
		query = readFile(queryDir + std::to_string(i) + ".sql");
		bool modified = false;
		query = preprocessTpchQuery(query, &modified);
		if (modified && !warned_templates) {
			std::cerr << "Warning: query templates detected; substituting default parameter values." << std::endl;
			warned_templates = true;
		}
		queries.push_back(query);
	}

	int queryCount = queryList.empty() ? 22 : queryList.size();

	for (int i = 0; i < queryCount * rounds; ++i) {
		std::cout << "\n>>> RUN ITERATION " << i << std::endl;
		int queryIdx = queryList.empty() ? i % 22 : queryList[i % queryCount] - 1;

		auto start = std::chrono::high_resolution_clock::now();
		std::cout << queries[queryIdx] << std::endl;
		auto result = con.Query(queries[queryIdx]);
		if (!result || result->HasError()) {
			throw std::runtime_error("Query execution failed: " + (result ? result->GetError() : "null result"));
		}
		auto end = std::chrono::high_resolution_clock::now();

		if (verbose) {
			result->Print();
		}
		std::cout << "\nSQL_OUT:" << result->ToString() << std::endl;

		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		if (duration.count() == 0) {
			zero_ms_count++;
		}
		if (verbose) {
			std::cout << "QUERY " << (queryIdx + 1) << ": " << duration.count() << " milliseconds" << std::endl;
		}

		totalQueries++;
		totalTime += duration.count() / 1000.0;
		latencies.push_back(std::pair<int, double>{queryIdx + 1, duration.count()});
	}

	stream_stats res;
	res.throughput = totalQueries / totalTime;
	res.latencies = latencies;
	if (zero_ms_count == totalQueries) {
		std::cerr << "Warning: all query timings were 0ms; timing resolution may be too coarse." << std::endl;
	}

	return res;
}


std::vector<double> individualTPCH(Connection &con, std::string dataDir, std::string queryDir, bool verbose) {

	std::string queries;
	int zero_ms_count = 0;
	bool warned_templates = false;

	std::cout << "\n>>> BEGIN TPC-H Individual Query Test:\n" << std::endl;

	std::cout << "\n>>> INIT SCHEMA... \n" << std::endl;
	queries = readFile(dataDir + "schema.sql");
#ifdef DEBUG
	std::cout << queries << std::endl;
#endif
	auto result = con.Query(queries);
	if (!result || result->HasError()) {
		throw std::runtime_error("Schema load failed: " + (result ? result->GetError() : "null result"));
	}

	std::cout << "\n>>> LOAD DATA... \n" << std::endl;
	queries = readFile(dataDir + "load.sql");
#ifdef DEBUG
	std::cout << queries << std::endl;
#endif
	result = con.Query(queries);
	if (!result || result->HasError()) {
		throw std::runtime_error("Data load failed: " + (result ? result->GetError() : "null result"));
	}

	std::vector<double> timings;

	for (int i = 1; i <= 22; ++i) {
		std::cout << "\n>>> RUN QUERY " << i << std::endl;

		queries = readFile(queryDir + std::to_string(i) + ".sql");
		bool modified = false;
		queries = preprocessTpchQuery(queries, &modified);
		if (modified && !warned_templates) {
			std::cerr << "Warning: query templates detected; substituting default parameter values." << std::endl;
			warned_templates = true;
		}
//#ifdef DEBUG
		std::cout << queries << std::endl;
//#endif
		auto start = std::chrono::high_resolution_clock::now();
		auto result = con.Query(queries);
		if (!result || result->HasError()) {
			throw std::runtime_error("Query execution failed: " + (result ? result->GetError() : "null result"));
		}
		auto end = std::chrono::high_resolution_clock::now();

		if (verbose) {
			result->Print();
		}

		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		if (duration.count() == 0) {
			zero_ms_count++;
		}
		if (verbose) {
			std::cout << "QUERY " << i << ": " << duration.count() << " milliseconds" << std::endl;
		}
#ifdef DEBUG
		std::cout << "QUERY " << i << ": " << duration.count() << " milliseconds" << std::endl;
#endif
		timings.push_back(duration.count());
	}

	for (int i = 1; i <= 22; ++i) {
		std::cout << "Iteration " << i << ": " << timings[i-1] << " milliseconds" << std::endl;
	}
	if (zero_ms_count == 22) {
		std::cerr << "Warning: all query timings were 0ms; timing resolution may be too coarse." << std::endl;
	}

	return timings;

}
