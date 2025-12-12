#include "ppgenerator/phrase_generator.hpp"

#include <array>
#include <random>
#include <sstream>
#include <string_view>

namespace PingPong
{
namespace
{
constexpr std::array<std::string_view, 46> c_dictionary = {
    "accismus",
    "acumen",
    "aglet",
    "anachronism",
    "aphotic",
    "aplomb",
    "behove",
    "cacophony",
    "cryptic",
    "doppelganger",
    "draconian",
    "ephemeral",
    "fecund",
    "frivol",
    "gambit",
    "garrulous",
    "iconoclast",
    "impetus",
    "intrepid",
    "juggernaut",
    "juxtaposition",
    "kismet",
    "makebate",
    "mendacious",
    "mettle",
    "murmuration",
    "nastify",
    "nefarious",
    "overmorrow",
    "paragon",
    "pessimum",
    "petrichor",
    "platitude",
    "puerile",
    "redame",
    "riposte",
    "sanguine",
    "sarcast",
    "serendipity",
    "solivagant",
    "sonder",
    "syzygy",
    "tidbit",
    "vagabond",
    "yaffle",
    "zephyr",
};
}

std::string getRandomPhrase(const size_t word_cnt)
{
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> idx_dist(0, c_dictionary.size() - 1);
    std::uniform_int_distribution<size_t> num_dist(0, 15);

    std::stringstream ss;

    for (auto i = 0; i < word_cnt; ++i)
    {
        ss << c_dictionary[idx_dist(rng)] << '-';
    }

    int rand_num1 = num_dist(rng);
    int rand_num2 = num_dist(rng);
    int rand_num3 = num_dist(rng);

    ss << std::hex << rand_num1 << rand_num2 << rand_num3;

    return ss.str();
}
} // namespace PingPong
