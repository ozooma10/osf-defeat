#include "Test.h"

int main(int argc, char** argv) {
    using namespace Test;
    Json report = {{"schemaVersion", 1}, {"cases", Json::array()}, {"passed", false}};
    int failures = 0;
    try {
        // Create a private directory atomically. Never place fixtures next to the real game.
        for (;;) {
            sandbox = std::filesystem::temp_directory_path() / ("osfdefeat-tests-" + std::to_string(std::random_device{}()));
            if (std::filesystem::create_directory(sandbox)) break;
        }
        TestEngine::executable = sandbox / "Starfield.exe";
        for (const auto& test : Cases()) {
            const int before = checks;
            try {
                Reset(); test.run();
                report["cases"].push_back({{"name", test.name}, {"passed", true}, {"checks", checks - before}});
                std::cout << "PASS " << test.name << '\n';
            } catch (const std::exception& e) {
                ++failures;
                report["cases"].push_back({{"name", test.name}, {"passed", false}, {"error", e.what()}, {"checks", checks - before}});
                std::cerr << "FAIL " << test.name << ": " << e.what() << '\n';
            }
        }
        report["passed"] = failures == 0 && !Cases().empty();
        report["checks"] = checks;
        if (argc == 3 && std::string_view(argv[1]) == "--report") {
            if (std::filesystem::exists(argv[2])) throw std::runtime_error("Report already exists");
            std::ofstream output(argv[2]); output << report.dump(2);
            if (!output) throw std::runtime_error("Cannot write report");
        } else if (argc != 1) throw std::runtime_error("Usage: test.exe [--report new-file.json]");
        std::filesystem::remove_all(sandbox); // Only the directory successfully created by this process.
        std::cout << Cases().size() << " cases, " << checks << " checks, " << failures << " failures\n";
        return report["passed"].get<bool>() ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
