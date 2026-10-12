#include "bench.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace benchmarks
{
    namespace
    {
        // The names used here are plain identifiers, but a description may contain anything.
        std::string json_string(const std::string &text)
        {
            std::string out = "\"";
            for (char c : text)
            {
                if (c == '"' || c == '\\')
                {
                    out += '\\';
                    out += c;
                }
                else if (static_cast<unsigned char>(c) < 0x20)
                {
                    out += ' ';
                }
                else
                {
                    out += c;
                }
            }
            return out + "\"";
        }

        std::string hex(uint64_t value)
        {
            char buffer[2 + 16 + 1];
            std::snprintf(buffer, sizeof(buffer), "0x%016llx", static_cast<unsigned long long>(value));
            return buffer;
        }

        std::string number(double value, int decimals)
        {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
            return buffer;
        }

        // The checksum is a string: a 64-bit integer does not survive a JSON reader that uses doubles.
        std::string json_record(const std::string &suite, const Case &c, const std::string &engine, const Result &r)
        {
            std::string out = "{";
            auto field = [&](const char *key, const std::string &value, bool last = false)
            {
                out += "\"" + std::string(key) + "\": " + value + (last ? "}" : ", ");
            };
            field("suite", json_string(suite));
            field("case", json_string(c.name()));
            field("group", json_string(c.group()));
            field("kind", json_string(c.kind()));
            field("engine", json_string(engine));
            field("iters", std::to_string(r.params.iters));
            field("warmup", std::to_string(r.params.warmup));
            field("seed", std::to_string(r.params.seed));
            field("checksum", json_string(hex(r.checksum)));
            field("changes", std::to_string(r.changes));
            field("sim_time", std::to_string(r.sim_time));
            field("counters", json_string(r.counts.source));
            field("multiplexed", r.counts.multiplexed ? "true" : "false");
            field("instructions", std::to_string(r.counts.instructions));
            field("cycles", std::to_string(r.counts.cycles));
            field("l1d_misses", std::to_string(r.counts.l1d_misses));
            field("branch_misses", std::to_string(r.counts.branch_misses));
            field("wall_ns", std::to_string(r.counts.wall_ns));
            field("cpu_ns", std::to_string(r.counts.cpu_ns));
            field("kernel_ns", std::to_string(r.counts.kernel_ns));
            field("build_ns", std::to_string(r.build_ns));
            field("start_ns", std::to_string(r.start_ns));
            field("peak_rss_kb", std::to_string(r.peak_rss_kb), true);
            return out;
        }

        void print_record(const std::string &suite, const Case &c, const std::string &engine, const Result &r)
        {
            const double n = static_cast<double>(r.params.iters ? r.params.iters : 1);
            const Counts &k = r.counts;
            std::cout << suite << " " << c.name() << " [" << engine << "]  iters " << r.params.iters << ", warmup "
                      << r.params.warmup << ", checksum " << hex(r.checksum) << " (" << r.changes << " changes)\n";
            std::cout << "  per iteration: ";
            if (k.instructions)
            {
                std::cout << number(static_cast<double>(k.instructions) / n, 1) << " instructions, ";
            }
            if (k.cycles)
            {
                std::cout << number(static_cast<double>(k.cycles) / n, 1) << " cycles, ";
            }
            if (k.instructions && k.cycles)
            {
                std::cout << "IPC " << number(static_cast<double>(k.instructions) / static_cast<double>(k.cycles), 2) << ", ";
            }
            if (k.l1d_misses)
            {
                std::cout << number(static_cast<double>(k.l1d_misses) / n, 2) << " L1d misses, ";
            }
            if (k.branch_misses)
            {
                std::cout << number(static_cast<double>(k.branch_misses) / n, 2) << " branch misses, ";
            }
            std::cout << number(static_cast<double>(k.wall_ns) / n, 1) << " ns\n";
            // The counters stop at the kernel. Say so when a design spends a visible part of its time there.
            if (k.cpu_ns != 0 && k.kernel_ns * 20 > k.cpu_ns)
            {
                std::cout << "  " << number(100.0 * static_cast<double>(k.kernel_ns) / static_cast<double>(k.cpu_ns), 0)
                          << "% of the CPU time was spent in the kernel, which the cycle and instruction counts leave out\n";
            }
            std::cout << "  measured " << number(static_cast<double>(k.wall_ns) * 1e-6, 1) << " ms (counters: " << k.source
                      << (k.multiplexed ? ", multiplexed" : "") << "), build " << number(static_cast<double>(r.build_ns) * 1e-6, 1)
                      << " ms, start " << number(static_cast<double>(r.start_ns) * 1e-6, 1) << " ms, peak RSS "
                      << number(static_cast<double>(r.peak_rss_kb) / 1024.0, 1) << " MB\n";
        }

        void print_usage(const char *program, const std::string &suite)
        {
            std::cerr << "Benchmark suite '" << suite << "'. Each run measures one engine of one case.\n\n"
                      << "  " << program << " --list [--json]\n"
                      << "  " << program << " --case NAME --engine ENGINE [--iters N] [--warmup N] [--seed N] [--json]\n"
                      << "      [--layout TEXT]\n\n"
                      << "--layout takes any text and ignores it. The arguments of a program are on its stack, and\n"
                      << "SystemC copies them to the heap, so their length moves everything that is allocated later.\n"
                      << "The same code can run several percent faster or slower depending on where its data lands,\n"
                      << "and a text of another length gives another memory layout to measure.\n\n"
                      << "scripts/benchmarks.py runs and compares all of them.\n";
        }

        uint64_t parse_count(const std::string &option, const char *text)
        {
            char *end = nullptr;
            const unsigned long long value = std::strtoull(text, &end, 10);
            if (end == text || *end != '\0')
            {
                throw std::invalid_argument(option + " needs a number, got '" + std::string(text) + "'");
            }
            return value;
        }
    } // namespace

    Case::Case(std::string group, std::string kind, Params params, std::string description)
        : group_(std::move(group)),
          kind_(std::move(kind)),
          params_(params),
          description_(std::move(description))
    {
        if (params_.warmup == 0)
        {
            params_.warmup = params_.iters / 10;
        }
    }

    Suite::Suite(std::string name) : name_(std::move(name))
    {
    }

    Case &Suite::add(std::string group, std::string kind, Params params, std::string description)
    {
        cases_.push_back(std::make_unique<Case>(std::move(group), std::move(kind), params, std::move(description)));
        return *cases_.back();
    }

    int Suite::main(int argc, char *argv[])
    {
        bool list = false;
        bool json = false;
        std::string case_name;
        std::string engine_name;
        // The case's own parameters, unless the command line gives them.
        bool has_iters = false, has_warmup = false, has_seed = false;
        Params overrides;

        try
        {
            for (int i = 1; i < argc; ++i)
            {
                const std::string option = argv[i];
                auto value = [&]() -> const char *
                {
                    if (i + 1 >= argc)
                    {
                        throw std::invalid_argument(option + " needs a value");
                    }
                    return argv[++i];
                };
                if (option == "--list")
                {
                    list = true;
                }
                else if (option == "--json")
                {
                    json = true;
                }
                else if (option == "--case")
                {
                    case_name = value();
                }
                else if (option == "--engine")
                {
                    engine_name = value();
                }
                else if (option == "--iters")
                {
                    overrides.iters = parse_count(option, value());
                    has_iters = true;
                }
                else if (option == "--warmup")
                {
                    overrides.warmup = parse_count(option, value());
                    has_warmup = true;
                }
                else if (option == "--layout")
                {
                    // Not used: what matters is how long the argument is. See print_usage().
                    (void)value();
                }
                else if (option == "--seed")
                {
                    overrides.seed = parse_count(option, value());
                    has_seed = true;
                }
                else
                {
                    throw std::invalid_argument("unknown option '" + option + "'");
                }
            }
        }
        catch (const std::invalid_argument &error)
        {
            std::cerr << error.what() << "\n\n";
            print_usage(argv[0], name_);
            return 2;
        }

        if (list)
        {
            for (const auto &c : cases_)
            {
                if (json)
                {
                    std::string engines;
                    for (const auto &engine : c->engines())
                    {
                        engines += (engines.empty() ? "" : ", ") + json_string(engine.name);
                    }
                    std::cout << "{\"suite\": " << json_string(name_) << ", \"case\": " << json_string(c->name())
                              << ", \"group\": " << json_string(c->group()) << ", \"kind\": " << json_string(c->kind())
                              << ", \"engines\": [" << engines << "], \"iters\": " << c->params().iters
                              << ", \"warmup\": " << c->params().warmup
                              << ", \"description\": " << json_string(c->description()) << "}\n";
                }
                else
                {
                    std::cout << c->name() << "  [";
                    for (const auto &engine : c->engines())
                    {
                        std::cout << (&engine == &c->engines().front() ? "" : ", ") << engine.name;
                    }
                    std::cout << "]  iters " << c->params().iters << ", warmup " << c->params().warmup;
                    if (!c->description().empty())
                    {
                        std::cout << "  " << c->description();
                    }
                    std::cout << "\n";
                }
            }
            return 0;
        }

        if (case_name.empty() || engine_name.empty())
        {
            print_usage(argv[0], name_);
            return 2;
        }

        for (const auto &c : cases_)
        {
            if (c->name() != case_name)
            {
                continue;
            }
            for (const auto &engine : c->engines())
            {
                if (engine.name != engine_name)
                {
                    continue;
                }
                Params params = c->params();
                if (has_iters)
                {
                    params.iters = overrides.iters;
                }
                if (has_warmup)
                {
                    params.warmup = overrides.warmup;
                }
                if (has_seed)
                {
                    params.seed = overrides.seed;
                }

                const Result result = engine.run(params);
                if (json)
                {
                    std::cout << json_record(name_, *c, engine.name, result) << std::endl;
                }
                else
                {
                    print_record(name_, *c, engine.name, result);
                }
                if (result.changes == 0)
                {
                    std::cerr << name_ << " " << case_name << " [" << engine_name
                              << "]: the checksummed value never changed. The design does nothing, or the run is "
                                 "shorter than its latency.\n";
                    return 3;
                }
                return 0;
            }
            std::cerr << "Case '" << case_name << "' has no engine '" << engine_name << "'.\n";
            return 2;
        }
        std::cerr << "Suite '" << name_ << "' has no case '" << case_name << "'. Use --list to see the cases.\n";
        return 2;
    }
}
