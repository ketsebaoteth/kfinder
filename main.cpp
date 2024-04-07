//////////////////////////////////////////////////////
// c++ gurus dont judge my code please !!🤣#
/////////////////////////////////////////////////////
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <future>
#include <thread>
#include <mutex>
#include <chrono>
#include <map>
#include <rapidfuzz/fuzz.hpp>
#include <condition_variable>
#include <queue>
#include <functional>
#include <stdexcept>
#include <set>
#include <conio.h>

class Semaphore
{
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;

public:
    Semaphore(int count_ = 0) : count(count_) {}

    void notify()
    {
        std::unique_lock<std::mutex> lock(mtx);
        ++count;
        cv.notify_one();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);
        while (count == 0)
        {
            cv.wait(lock);
        }
        --count;
    }
};

Semaphore sem(8);

std::mutex mtx;
std::mutex indexMutex;

class ThreadPool
{
public:
    ThreadPool(size_t);
    template <class F, class... Args>
    auto enqueue(F &&f, Args &&...args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();

private:
    // need to keep track of threads so we can join them
    std::vector<std::thread> workers;
    // the task queue
    std::queue<std::function<void()>> tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    : stop(false)
{
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back(
            [this]
            {
                for (;;)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                                             [this]
                                             { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            });
}

// add new work item to the pool
template <class F, class... Args>
auto ThreadPool::enqueue(F &&f, Args &&...args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task]()
                      { (*task)(); });
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers)
        worker.join();
}

void sortFile(const std::string &path)
{
    // Read lines from file
    std::ifstream fileIn(path);
    std::vector<std::string> lines;
    std::string line;
    std::cout << "reading files" << std::endl;
    while (std::getline(fileIn, line))
    {
        std::cout << line << std::endl;
        lines.push_back(line);
    }
    fileIn.close();

    // Sort lines
    std::sort(lines.begin(), lines.end(), [](const std::string &a, const std::string &b)
              {
    std::string aSub = a.substr(0, a.find(":"));
    std::string bSub = b.substr(0, b.find(":"));
    std::transform(aSub.begin(), aSub.end(), aSub.begin(), ::tolower);
    std::transform(bSub.begin(), bSub.end(), bSub.begin(), ::tolower);
    return aSub < bSub; });
    // Write sorted lines to file
    std::ofstream fileOut(path);
    for (const auto &sortedLine : lines)
    {
        fileOut << sortedLine << '\n';
    }
}

void sortFilesInIndex()
{
    std::vector<std::future<void>> futures;
    for (const auto &entry : std::filesystem::directory_iterator("index/"))
    {
        if (entry.path().extension() == ".txt")
        {
            futures.push_back(std::async(std::launch::async, sortFile, entry.path().string()));
        }
    }
    for (auto &future : futures)
    {
        future.get();
    }
}

void init()
{
    // Create index directory if it doesn't exist
    if (!std::filesystem::exists("index"))
    {
        std::filesystem::create_directory("index");
    }

    // Create a-z files in index directory
    for (char c = 'a'; c <= 'z'; ++c)
    {
        std::ofstream("index/" + std::string(1, c) + ".txt").close();
    }
}

std::mutex indexMapMutex;

void indexDirectory(const std::filesystem::path &path, std::atomic<int> &fileCount, std::unordered_map<char, std::string> &indexMap, std::atomic<int> &dirCount)
{
    for (const auto &entry : std::filesystem::directory_iterator(path))
    {
        char firstChar = std::tolower(entry.path().filename().string()[0]);
        {
            std::lock_guard<std::mutex> lock(indexMapMutex);
            indexMap[firstChar] += entry.path().filename().string() + " : " + entry.path().string() + '\n';
        }
        if (entry.is_regular_file())
        {
            ++fileCount;
        }
        else if (entry.is_directory())
        {
            ++dirCount;
            indexDirectory(entry.path(), fileCount, indexMap, dirCount);
        }
    }
}

void indexFile(const std::filesystem::path &path, std::atomic<int> &fileCount, std::unordered_map<char, std::string> &indexMap)
{
    char firstChar = std::tolower(path.filename().string()[0]);
    std::string filename = path.filename().string();
    std::string filepath = path.string();
    {
        std::lock_guard<std::mutex> lock(indexMapMutex);
        indexMap[firstChar] += filename + " : " + filepath + '\n';
    }
    ++fileCount;
}

void index(std::string path)
{
    std::atomic<int> fileCount = 0;
    std::atomic<int> dirCount = 0;

    // Create a map to store the index in memory
    std::unordered_map<char, std::string> indexMap;

    // Check if index folder exists, if not, initialize it
    if (!std::filesystem::exists("index"))
    {
        init();
    }

    ThreadPool pool(std::thread::hardware_concurrency());

    for (auto it = std::filesystem::recursive_directory_iterator(path); it != std::filesystem::recursive_directory_iterator();)
    {
        std::filesystem::directory_entry entry;
        try
        {
            entry = *it;
            ++it;
        }
        catch (const std::filesystem::filesystem_error &err)
        {
            std::cerr << "Ignorable Permission Error accessing file/directory: " << err.what() << '\n';
            it.disable_recursion_pending(); // Don't recurse into this directory
            ++it;
            continue;
        }

        if (entry.is_directory())
        {
            ++dirCount;
            pool.enqueue(indexDirectory, entry, std::ref(fileCount), std::ref(indexMap), std::ref(dirCount));
        }
        else
        {
            pool.enqueue(indexFile, entry, std::ref(fileCount), std::ref(indexMap));
            ++fileCount;
        }
    }

    // Write the index to the files
    std::vector<std::future<void>> writeFutures;
    for (auto &[key, value] : indexMap)
    {
        writeFutures.push_back(pool.enqueue([key, &value]
                                            {
            std::ofstream indexFile("index/" + std::string(1, key) + ".txt", std::ios::app);
            if (!indexFile)
            {
                std::cerr << "Ignorable Permission Error: Could not open file "
                          << "index/" + std::string(1, key) + ".txt" << '\n';
                return;
            }
            indexFile << value;
            indexFile.close(); }));
    }
    for (auto &future : writeFutures)
    {
        future.get();
    }

    std::cout << "Indexed " << fileCount << " files and " << dirCount << " directories\n";
}

int levenshteinDistance(const std::string &s1, const std::string &s2)
{
    int m = s1.size(), n = s2.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (int i = 1; i <= m; i++)
        dp[i][0] = i;
    for (int j = 1; j <= n; j++)
        dp[0][j] = j;

    for (int i = 1; i <= m; i++)
    {
        for (int j = 1; j <= n; j++)
        {
            if (s1[i - 1] == s2[j - 1])
                dp[i][j] = dp[i - 1][j - 1];
            else
                dp[i][j] = std::min({dp[i - 1][j - 1], dp[i - 1][j], dp[i][j - 1]}) + 1;
        }
    }

    return dp[m][n];
}

double jaroWinklerSimilarity(const std::string &s1, const std::string &s2)
{
    int m = s1.size(), n = s2.size();
    if (m == 0 || n == 0)
        return 0;

    int match_distance = std::max(m, n) / 2 - 1;
    std::vector<bool> s1_matches(s1.size(), false);
    std::vector<bool> s2_matches(s2.size(), false);

    int matches = 0;
    int transpositions = 0;

    for (int i = 0; i < m; i++)
    {
        int start = std::max(0, i - match_distance);
        int end = std::min(i + match_distance + 1, n);

        for (int j = start; j < end; j++)
        {
            if (s2_matches[j])
                continue;
            if (s1[i] != s2[j])
                continue;
            s1_matches[i] = true;
            s2_matches[j] = true;
            matches++;
            break;
        }
    }

    if (matches == 0)
        return 0;

    int k = 0;
    for (int i = 0; i < m; i++)
    {
        if (!s1_matches[i])
            continue;
        while (!s2_matches[k])
            k++;
        if (s1[i] != s2[k])
            transpositions++;
        k++;
    }

    double jaro = ((matches / (double)m) + (matches / (double)n) + ((matches - transpositions / 2.0) / matches)) / 3.0;
    int prefix = 0;
    for (int i = 0; i < std::min(m, n); i++)
    {
        if (s1[i] == s2[i])
            prefix++;
        else
            break;
    }

    return jaro + std::min(0.1, 1.0 / std::max(m, n)) * prefix * (1 - jaro);
}

bool fuzzymatch(const std::string &query, const std::string &target, double minSimilarity)
{
    double similarity = rapidfuzz::fuzz::ratio(query, target);
    return similarity >= minSimilarity;
}
std::vector<std::pair<std::string, double>> advancedsearch(std::string query, bool shouldPrompt = false)
{
    auto start = std::chrono::high_resolution_clock::now();
    // Convert query to lowercase
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

    // Determine the file to open
    char firstchar = std::tolower(query[0]);
    std::string pathtoassociatedfile = "index/" + std::string(1, firstchar) + ".txt";

    // Read lines from file
    std::ifstream fileIn(pathtoassociatedfile);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(fileIn, line))
    {
        lines.push_back(line);
    }
    fileIn.close();

    // Perform fuzzy search on all lines
    std::vector<std::pair<std::string, double>> matches;
    for (const auto &line : lines)
    {
        std::string aSub = line.substr(0, line.find(":"));
        aSub.erase(std::remove_if(aSub.begin(), aSub.end(), ::isspace), aSub.end());
        std::transform(aSub.begin(), aSub.end(), aSub.begin(), ::tolower);
        double score = rapidfuzz::fuzz::ratio(query, aSub); // calculate similarity score
        if (score > 0.3)                                    // only keep matches with score > 0.5
        {
            matches.push_back({line, score});
        }
    }

    // Sort matches based on score
    std::sort(matches.begin(), matches.end(), [](const auto &a, const auto &b)
              {
                  return a.second > b.second; // sort in descending order of score
              });

    // Return top 30 matches
    if (matches.size() > 30)
    {
        matches.resize(30);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    std::cout << "\033[32m"
              << "search took " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << " ms"
              << "\033[0m" << std::endl;

    return matches;
}

int main(int argc, char **argv)
{
    bool shouldrun = true;
    bool searchMode = false;
    std::string searchQuery;
    int previousMatchCount = 0;
    bool firsttime = true;
    std::vector<std::pair<std::string, double>> searchResults;
    std::cout << R"(

 .----------------. .----------------. .----------------. .-----------------..----------------. .----------------. .----------------. 
| .--------------. | .--------------. | .--------------. | .--------------. | .--------------. | .--------------. | .--------------. |
| |  ___  ____   | | |  _________   | | |     _____    | | | ____  _____  | | |  ________    | | |  _________   | | |  _______     | |
| | |_  ||_  _|  | | | |_   ___  |  | | |    |_   _|   | | ||_   \|_   _| | | | |_   ___ `.  | | | |_   ___  |  | | | |_   __ \    | |
| |   | |_/ /    | | |   | |_  \_|  | | |      | |     | | |  |   \ | |   | | |   | |   `. \ | | |   | |_  \_|  | | |   | |__) |   | |
| |   |  __'.    | | |   |  _|      | | |      | |     | | |  | |\ \| |   | | |   | |    | | | | |   |  _|  _   | | |   |  __ /    | |
| |  _| |  \ \_  | | |  _| |_       | | |     _| |_    | | | _| |_\   |_  | | |  _| |___.' / | | |  _| |___/ |  | | |  _| |  \ \_  | |
| | |____||____| | | | |_____|      | | |    |_____|   | | ||_____|\____| | | | |________.'  | | | |_________|  | | | |____| |___| | |
| |              | | |              | | |              | | |              | | |              | | |              | | |              | |
| '--------------' | '--------------' | '--------------' | '--------------' | '--------------' | '--------------' | '--------------' |
 '----------------' '----------------' '----------------' '----------------' '----------------' '----------------' '----------------' 
                       
    )" << '\n';
    while (true)
    {
        if (!searchMode)
        {
            std::cout << "\nPlease enter a command:\n"
                      << "  init - Initialize the index\n"
                      << "  index - Index a specified path\n"
                      << "  search - Enter search mode\n"
                      << "  quit - Quit the program\n";

            std::string command;
            std::getline(std::cin, command);

            if (command == "quit")
            {
                std::cout << "Exiting the program. Goodbye!\n";
                break;
            }
            else if (command == "init")
            {
                init();
                std::cout << "Index initialized.\n";
            }
            else if (command == "index")
            {
                std::cout << "Enter the path to index: ";
                std::string path;
                std::getline(std::cin, path);
                std::cout << "Indexing the path: " << path << "\n";
                auto start = std::chrono::high_resolution_clock::now();
                index(path);
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = end - start;
                std::cout << "Indexing completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << " ms\n";
            }
            else if (command == "search")
            {
                searchMode = true;
                searchQuery.clear();
                std::cout << "Entering search mode. Type your query and press Enter to execute it.\n";
            }
            else
            {
                std::cout << "Invalid command. Please try again.\n";
            }
        }
        else if (searchMode)
        {
            // Clear the previous search results
            searchResults.clear();

            // Store the search results in a variable
            searchResults = advancedsearch(searchQuery, false);

            // Clear the console
            system("cls");

            // Print the current search query
            std::cout << "Search query: " << searchQuery << "\n";

            for (size_t i = 0; i < std::min(searchResults.size(), size_t(20)); i++)
            {
                std::cout << i + 1 << ". " << searchResults[i].first << ": " << searchResults[i].second << "\n";
            }

            char ch;
            ch = _getch();
            if (ch == '\r')
            {
                system("cls");
                searchResults = advancedsearch(searchQuery, true);
                for (size_t i = 0; i < std::min(searchResults.size(), size_t(20)); i++)
                {
                    std::cout << i + 1 << ". " << searchResults[i].first << ": " << searchResults[i].second << "\n";
                }
                std::cout << "Enter the number of the file you want to open, or 'q' to quit: ";
                std::string input;
                std::getline(std::cin, input);
                if (input == "q")
                {
                    searchMode = false;
                }
                else
                {
                    try
                    {
                        int index = std::stoi(input);
                        if (index > 0 && index <= searchResults.size())
                        {
                            std::string filepath = searchResults[index - 1].first.substr(searchResults[index - 1].first.find(":") + 2);
                            std::filesystem::path path(filepath);
                            path = std::filesystem::absolute(path); // convert to absolute path
                            std::cout << "Opening file: " << path.string() << "\n";
                            if (std::filesystem::is_directory(path))
                            {
                                std::string command = "explorer \"" + path.string() + "\"";
                                system(command.c_str());
                                searchMode = false;
                            }
                            else
                            {
                                std::string command = "start \"\" \"" + path.string() + "\"";
                                system(command.c_str());
                                searchMode = false;
                            }
                        }
                        else
                        {
                            std::cout << "Invalid number. Please try again.\n";
                        }
                    }
                    catch (const std::exception &e)
                    {
                        std::cout << "Invalid input. Please try again.\n";
                    }
                }
            }
            else if (ch == '\b')
            {
                if (!searchQuery.empty())
                {
                    searchQuery.pop_back();
                }
            }
            else
            {
                searchQuery.push_back(ch);
            }
        }
    }

    return 0;
};