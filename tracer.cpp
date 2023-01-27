// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4;
// indent-tabs-mode: nil; -*-

/* tracer - Utility for printing UPPAAL XTR trace files.
   Copyright (C) 2006 Uppsala University and Aalborg University.
   Copyright (C) 2017 Aalborg University.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA
*/

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>

/** This utility takes an UPPAAL model in the UPPAAL intermediate
 * format and a UPPAAL XTR trace file and prints trace to stdout in a
 * human readable format.
 *
 * The utility basically contains two parsers: One for the
 * intermediate format and one for the XTR format. You may want to use
 * them a starting point for writing analysis tools.
 *
 * Notice that the intermediate format uses a global numbering of
 * clocks, variables, locations, etc. This is in contrast to the XTR
 * format, which makes a clear distinction between e.g. clocks and
 * variables and uses process local number of locations and
 * edges. Care must be taken to convert between these two numbering
 * schemes.
 */

using std::cout;
using std::endl;

/** Representation of a memory cell. */
struct cell_t
{
    enum type_t : int { CONST, CLOCK, VAR, META, SYS_META, COST, LOCATION, FIXED };
    enum flags_t : int { NONE, COMMITTED, URGENT };
    /** The type of the cell. */
    type_t type;

    /** Name of cell. Not all types have names. */
    std::string name;

    union
    {
        int value;
        struct
        {
            int nr;
        } clock;
        struct
        {
            int min;
            int max;
            int init;
            int nr;
        } var;
        struct
        {
            int min;
            int max;
            int init;
            int nr;
        } meta;
        struct
        {
            int min;
            int max;
        } sys_meta;
        struct
        {
            flags_t flags;
            int process;
            int invariant;
        } location;
        struct
        {
            int min;
            int max;
        } fixed;
    };
};

/** Representation of a process. */
struct process_t
{
    int initial{-1};
    std::string name;
    std::vector<int> locations;
    std::vector<int> edges;
};

/** Representation of an edge. */
struct edge_t
{
    int process{-1};
    int source{-1};
    int target{-1};
    int guard{-1};
    int sync{-1};
    int update{-1};
};

/** The UPPAAL model in intermediate format. */
static std::vector<cell_t> layout;
static std::vector<int> instructions;
static std::vector<process_t> processes;
static std::vector<edge_t> edges;
static std::map<int, std::string> expressions;

/** For convenience we keep the size of the system here. */
static size_t processCount = 0;
static size_t variableCount = 0;
static size_t clockCount = 0;

/** These are mappings from variable and clock indices to
 * the names of these variables and clocks.
 */
static std::vector<std::string> clocks;
static std::vector<std::string> variables;

/** Thrown by parser upon parse errors. */
class invalid_format : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/** Reads one line from file. Skips comments. */
static bool read(std::istream& file, std::string& str)
{
    do {
        if (!getline(file, str)) {
            return false;
        }
    } while (!str.empty() && str[0] == '#');
    return true;
}

/** Reads one line and asserts that it contains a (terminating) dot */
static std::istream& read_dot(std::istream& is)
{
    std::string str;
    getline(is, str);
    if (std::all_of(str.begin(), str.end(), isspace))
        getline(is, str);  // skip white-spaces if any
    if (str != ".") {
        std::cerr << "Expecting a line with '.' but got '" << str << "'" << endl;
        assert(false);
        exit(EXIT_FAILURE);
    }
    return is;
}

inline std::istream& skip_spaces(std::istream& is)
{
    while (is.peek() == ' ')
        is.get();
    return is;
}

/** Parses intermediate format. */
static void loadIF(std::istream& file)
{
    std::string str;
    std::string section;
    char name[128];
    int index;

    while (getline(file, section)) {
        if (section == "layout") {
            cell_t cell;
            while (read(file, str) && !str.empty() && (isspace(str[0]) == 0)) {
                char s[6];
                const auto* cstr = str.c_str();

                if (sscanf(cstr, "%d:clock:%d:%31s", &index, &cell.clock.nr, name) == 3) {
                    cell.type = cell_t::CLOCK;
                    cell.name = name;
                    clocks.emplace_back(name);
                    clockCount++;
                } else if (sscanf(cstr, "%d:const:%d", &index, &cell.value) == 2) {
                    cell.type = cell_t::CONST;
                } else if (sscanf(cstr, "%d:var:%d:%d:%d:%d:%31s", &index, &cell.var.min, &cell.var.max, &cell.var.init,
                                  &cell.var.nr, name) == 6) {
                    cell.type = cell_t::VAR;
                    cell.name = name;
                    variables.emplace_back(name);
                    variableCount++;
                } else if (sscanf(cstr, "%d:meta:%d:%d:%d:%d:%31s", &index, &cell.meta.min, &cell.meta.max,
                                  &cell.meta.init, &cell.meta.nr, name) == 6) {
                    cell.type = cell_t::META;
                    cell.name = name;
                    variables.emplace_back(name);
                    variableCount++;
                } else if (sscanf(cstr, "%d:sys_meta:%d:%d:%31s", &index, &cell.sys_meta.min, &cell.sys_meta.max,
                                  name) == 4) {
                    cell.type = cell_t::SYS_META;
                    cell.name = name;
                } else if (sscanf(cstr, "%d:location::%31s", &index, name) == 2) {
                    cell.type = cell_t::LOCATION;
                    cell.location.flags = cell_t::NONE;
                    cell.name = name;
                } else if (sscanf(cstr, "%d:location:committed:%31s", &index, name) == 2) {
                    cell.type = cell_t::LOCATION;
                    cell.location.flags = cell_t::COMMITTED;
                    cell.name = name;
                } else if (sscanf(cstr, "%d:location:urgent:%31s", &index, name) == 2) {
                    cell.type = cell_t::LOCATION;
                    cell.location.flags = cell_t::URGENT;
                    cell.name = name;
                } else if (sscanf(cstr, "%d:static:%d:%d:%31s", &index, &cell.fixed.min, &cell.fixed.max, name) == 4) {
                    cell.type = cell_t::FIXED;
                    cell.name = name;
                } else if (sscanf(cstr, "%d:%5s", &index, s) == 2 && strcmp(s, "cost") == 0) {
                    cell.type = cell_t::COST;
                } else {
                    throw invalid_format(str);
                }

                layout.push_back(cell);
            }
#if defined(ENABLE_CORA) || defined(ENABLE_PRICED)
            cell.type = cell_t::VAR;
            cell.var.min = std::numeric_limits<int32_t>::min();
            cell.var.max = std::numeric_limits<int32_t>::max();
            cell.var.init = 0;

            cell.name = "infimum_cost";
            cell.var.nr = variableCount++;
            variables.push_back(cell.name);
            layout.push_back(cell);

            cell.name = "offset_cost";
            cell.var.nr = variableCount++;
            variables.push_back(cell.name);
            layout.push_back(cell);

            for (size_t i = 1; i < clocks.size(); ++i) {
                cell.name = "#rate[";
                cell.name.append(clocks[i]);
                cell.name.append("]");
                cell.var.nr = variableCount++;
                variables.push_back(cell.name);
                layout.push_back(cell);
            }
#endif
        } else if (section == "instructions") {
            while (read(file, str) && !str.empty() && ((isspace(str[0]) == 0) || str[0] == '\t')) {
                int address;
                int values[4];
                if (str[0] == '\t') {  // skip pretty-printed instruction text
                    continue;
                }
                int cnt = sscanf(str.c_str(), "%d:%d%d%d%d", &address, &values[0], &values[1], &values[2], &values[3]);
                if (cnt < 2) {
                    throw invalid_format("In instruction section");
                }

                for (int i = 0; i < cnt - 1; ++i) {
                    instructions.push_back(values[i]);
                }
            }
        } else if (section == "processes") {
            while (read(file, str) && !str.empty() && (isspace(str[0]) == 0)) {
                process_t process;
                if (sscanf(str.c_str(), "%d:%d:%31s", &index, &process.initial, name) != 3) {
                    throw invalid_format("In process section");
                }
                process.name = name;
                processes.push_back(process);
                processCount++;
            }
        } else if (section == "locations") {
            while (read(file, str) && !str.empty() && (isspace(str[0]) == 0)) {
                int index;
                int process;
                int invariant;

                if (sscanf(str.c_str(), "%d:%d:%d", &index, &process, &invariant) != 3)
                    throw invalid_format("In location section");

                layout[index].location.process = process;
                layout[index].location.invariant = invariant;
                processes[process].locations.push_back(index);
            }
        } else if (section == "edges") {
            while (read(file, str) && !str.empty() && (isspace(str[0]) == 0)) {
                edge_t edge;

                if (sscanf(str.c_str(), "%d:%d:%d:%d:%d:%d", &edge.process, &edge.source, &edge.target, &edge.guard,
                           &edge.sync, &edge.update) != 6) {
                    throw invalid_format("In edge section");
                }

                processes[edge.process].edges.push_back(edges.size());
                edges.push_back(edge);
            }
        } else if (section == "expressions") {
            while (read(file, str) && !str.empty() && (isspace(str[0]) == 0)) {
                if (sscanf(str.c_str(), "%d", &index) != 1) {
                    throw invalid_format("In expression section");
                }

                /* Find expression string (after the third colon). */
                const auto* s = str.c_str();
                int cnt = 3;
                while ((cnt != 0) && (*s != 0)) {
                    cnt -= static_cast<int>(*s == ':');
                    s++;
                }
                if (cnt != 0) {
                    throw invalid_format("In expression section");
                }

                /* Trim white space. */
                while ((*s != 0) && (isspace(*s) != 0)) {
                    s++;
                }
                const auto* t = s + strlen(s) - 1;
                while (t >= s && (isspace(*t) != 0)) {
                    t--;
                }

                expressions[index] = std::string(s, t + 1);
            }
        } else {
            throw invalid_format("Unknown section");
        }
    }
}

/** A bound for a clock constraint. A bound consists of a value and a
 * bit indicating whether the bound is strict or not.
 */
struct bound_t
{
    int value : 31;   // The value of the bound
    bool strict : 1;  // True if the bound is strict
};

/** The bound (infinity, <). */
static constexpr bound_t infinity = {std::numeric_limits<int32_t>::max() >> 1, true};

/** The bound (0, <=). */
static constexpr bound_t zero = {0, false};

/** A symbolic state. A symbolic state consists of a location vector, a
 * variable vector and a zone describing the possible values of the
 * clocks in a symbolic manner.
 */
class State
{
public:
    State();
    explicit State(std::istream& file);
    State(const State& s) = delete;
    State(State&& s) = delete;
    ~State() = default;

    int& get_location(size_t i) { return locations[i]; }
    int& get_variable(size_t i) { return integers[i]; }
    void set_bound(int i, int j, const bound_t& bound) { dbm[i * clockCount + j] = bound; }

    int get_location(size_t i) const { return locations[i]; }
    int get_variable(size_t i) const { return integers[i]; }
    const bound_t& get_bound(size_t i, size_t j) const { return dbm[i * clockCount + j]; }

private:
    std::vector<int> locations;
    std::vector<int> integers;
    std::vector<bound_t> dbm;
};

State::State(): locations(processCount), integers(variableCount), dbm(clockCount * clockCount, infinity)
{
    // Set diagonal and lower bounds to zero
    for (int i = 0; i < clockCount; ++i) {
        set_bound(0, i, zero);
        set_bound(i, i, zero);
    }
}

State::State(std::istream& file): State()
{
    // Read locations.
    for (auto& l : locations)
        file >> l;
    file >> read_dot;

    // Read DBM.
    int i, j, bnd;
    while (file >> i >> j >> bnd) {
        file >> read_dot;
        set_bound(i, j, {bnd >> 1, ((bnd & 1) != 0)});
    }
    file.clear();
    file >> read_dot;

    // Read integers.
    for (auto& v : integers)
        file >> v;
    file >> read_dot;
}

struct Edge
{
    int process{-1};
    int edge{-1};
    std::vector<int> select{};
};

/** A transition consists of one or more edges. Edges are indexes from
 * 0 in the order they appear in the input file.
 */
struct Transition
{
    std::vector<Edge> edges{};
    explicit Transition(std::istream& is);
};

Transition::Transition(std::istream& is)
{
    int process, edge, select;
    while (is >> process >> edge) {
        auto e = Edge{process, edge};
        is >> skip_spaces;
        while (is.peek() != '\n' && is.peek() != ';') {
            if (is >> select) {
                e.select.push_back(select);
            } else {
                std::cerr << "Transition format error" << endl;
                exit(EXIT_FAILURE);
            }
            is >> skip_spaces;
        }
        if (is.get() == '\n')  // old format without ';'
            --e.edge;          // old format indexes edges from 1, hence convert to 0-base
        edges.push_back(std::move(e));
    }
    is.clear();
    is >> read_dot;
}

/** Output operator for a symbolic state. Prints the location vector,
 * the variables and the zone of the symbolic state.
 */
static std::ostream& operator<<(std::ostream& o, const State& state)
{
    // Print location vector.
    for (size_t p = 0; p < processCount; p++) {
        int idx = processes[p].locations[state.get_location(p)];
        cout << processes[p].name << '.' << layout[idx].name << " ";
    }

    // Print variables.
    for (size_t v = 0; v < variableCount; v++) {
        cout << variables[v] << "=" << state.get_variable(v) << ' ';
    }

    // Print clocks.
    for (size_t i = 0; i < clockCount; i++) {
        for (size_t j = 0; j < clockCount; j++) {
            if (i != j) {
                const bound_t& bnd = state.get_bound(i, j);
                if (bnd.value != infinity.value) {
                    cout << clocks[i] << "-" << clocks[j] << (bnd.strict ? "<" : "<=") << bnd.value << " ";
                }
            }
        }
    }

    return o;
}

/** Output operator for a transition. Prints all edges in the
 * transition including the source, destination, guard,
 * synchronisation and assignment.
 */
static std::ostream& operator<<(std::ostream& o, const Transition& t)
{
    for (const auto& edge : t.edges) {
        int eid = processes[edge.process].edges[edge.edge];
        int src = edges[eid].source;
        int dst = edges[eid].target;
        int guard = edges[eid].guard;
        int sync = edges[eid].sync;
        int update = edges[eid].update;
        cout << processes[edge.process].name << '.' << layout[src].name << " -> " << processes[edge.process].name << '.'
             << layout[dst].name;
        if (!edge.select.empty()) {
            auto s = edge.select.begin(), se = edge.select.end();
            cout << " [" << *s;
            while (++s != se)
                cout << "," << *s;
            cout << "]";
        }
        cout << " {" << expressions[guard] << "; " << expressions[sync] << "; " << expressions[update] << ";} ";
    }

    return o;
}

/** Read and print a trace file. */
static void loadTrace(std::istream& is)
{
    // Read and print trace.
    cout << "State: " << State{is} << endl;
    for (;;) {
        // Skip white space.
        is >> skip_spaces;

        // A dot terminates the trace.
        if (is.peek() == '.') {
            is.get();
            break;
        }

        // Read a state and a transition.
        auto state = State{is};
        auto transition = Transition{is};

        // Print transition and state.
        cout << "\nTransition: " << transition << endl << "\nState: " << state << endl;
    }
}

int main(int argc, char* argv[])
{
    try {
        if (argc < 3) {
            printf("Synopsis: %s <if> <trace>\n", argv[0]);
            std::exit(EXIT_FAILURE);
        }

        // Load model in intermediate format.
        if (strcmp(argv[1], "-") == 0) {
            loadIF(std::cin);
        } else {
            auto file = std::ifstream{argv[1]};
            if (file.fail()) {
                perror(argv[1]);
                exit(EXIT_FAILURE);
            }
            loadIF(file);
            file.close();
        }

        // Load trace.
        auto file = std::ifstream{argv[2]};
        if (file.fail()) {
            perror(argv[2]);
            exit(EXIT_FAILURE);
        }
        loadTrace(file);
        file.close();
    } catch (std::exception& e) {
        std::cerr << "Caught exception: " << e.what() << endl;
    }
}
