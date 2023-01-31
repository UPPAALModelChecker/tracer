/** tracer - Utility for printing UPPAAL XTR trace files.
   Copyright (C) 2017-2023 Aalborg University.
   Copyright (C) 2006 Uppsala University and Aalborg University.

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

#include "tracer.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
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
 * them as a starting point for writing analysis tools.
 *
 * Notice that the intermediate format uses a global numbering of
 * clocks, integers, locations, etc. This is in contrast to the XTR
 * format, which makes a clear distinction between e.g. clocks and
 * integers and uses process local number of locations and
 * edges. Care must be taken to convert between these two numbering
 * schemes.
 */

using std::endl;

/** Thrown by parser upon parse errors. */
class invalid_format : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/** Reads one line from file. Skips comments. */
static bool read_line(std::istream& file, std::string& str)
{
    do {
        if (!std::getline(file, str)) {
            return false;
        }
    } while (!str.empty() && str[0] == '#');
    return true;
}

/** Reads one line and asserts that it contains a (terminating) dot */
static std::istream& read_dot(std::istream& is)
{
    std::string str;
    std::getline(is, str);
    if (std::all_of(str.begin(), str.end(), isspace))
        std::getline(is, str);  // skip white-spaces if any
    if (is.eof())
        throw invalid_format{"Expecting a dot ('.') but got EOF"};
    if (str != ".")
        throw invalid_format{"Expecting a dot ('.') but got '" + str + "'"};
    return is;
}

inline std::istream& skip_spaces(std::istream& is)
{
    while (is.peek() == ' ')
        is.get();
    return is;
}

/** Parses intermediate format. */
std::istream& model_t::read(std::istream& is)
{
    clear();
    std::string str;
    std::string section;
    int index;
    int mn, mx, init, nr, value;
    int process, invariant;
    int source, target, guard, sync, update;
    char name[128];

    while (std::getline(is, section)) {
        if (section == "layout") {
            auto cell = cell_t{};
            while (read_line(is, str) && !str.empty() && (isspace(str[0]) == 0)) {
                char s[6];
                const auto* cstr = str.c_str();
                if (sscanf(cstr, "%d:clock:%d:%31s", &index, &nr, name) == 3) {
                    cell.name = name;
                    cell.data = cell_t::clock_t{nr};
                    this->clocks.push_back(name);
                } else if (sscanf(cstr, "%d:const:%d", &index, &value) == 2) {
                    cell.data = cell_t::const_t{value};
                } else if (sscanf(cstr, "%d:var:%d:%d:%d:%d:%31s", &index, &mn, &mx, &init, &nr, name) == 6) {
                    cell.name = name;
                    cell.data = cell_t::integer_t{mn, mx, init, nr};
                    this->integers.push_back(name);
                } else if (sscanf(cstr, "%d:meta:%d:%d:%d:%d:%31s", &index, &mn, &mx, &init, &nr, name) == 6) {
                    cell.name = name;
                    cell.data = cell_t::meta_t{mn, mx, init, nr};
                    this->integers.emplace_back(name);
                } else if (sscanf(cstr, "%d:sys_meta:%d:%d:%31s", &index, &mn, &mx, name) == 4) {
                    cell.name = name;
                    cell.data = cell_t::sys_meta_t{mn, mx};
                } else if (sscanf(cstr, "%d:location::%31s", &index, name) == 2) {
                    cell.name = name;
                    cell.data = cell_t::location_t{cell_t::NONE};
                } else if (sscanf(cstr, "%d:location:committed:%31s", &index, name) == 2) {
                    cell.name = name;
                    cell.data = cell_t::location_t{cell_t::COMMITTED};
                } else if (sscanf(cstr, "%d:location:urgent:%31s", &index, name) == 2) {
                    cell.name = name;
                    cell.data = cell_t::location_t{cell_t::URGENT};
                } else if (sscanf(cstr, "%d:static:%d:%d:%31s", &index, &mn, &mx, name) == 4) {
                    cell.name = name;
                    cell.data = cell_t::fixed_t{mn, mx};
                } else if (sscanf(cstr, "%d:%5s", &index, s) == 2 && strcmp(s, "cost") == 0) {
                    cell.data = cell_t::cost_t{};
                } else {
                    throw invalid_format(str);
                }
                this->layout.push_back(std::move(cell));
            }
#if defined(ENABLE_CORA) || defined(ENABLE_PRICED)
            cell.name = "infimum_cost";
            cell.data = cell_t::integer_t{std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), 0,
                                          (int)model.integers.size()};
            model.integers.push_back(cell.name);
            model.layout.push_back(cell);

            cell.name = "offset_cost";
            cell.data = cell_t::integer_t{std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), 0,
                                          (int)model.integers.size()};
            model.integers.push_back(cell.name);
            model.layout.push_back(cell);

            for (size_t i = 1; i < model.clocks.size(); ++i) {
                cell.name = "#rate[";
                cell.name.append(model.clocks[i]);
                cell.name.append("]");
                cell.data = cell_t::integer_t{std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(),
                                              0, (int)model.integers.size()};
                model.integers.push_back(cell.name);
                model.layout.push_back(cell);
            }
#endif
        } else if (section == "instructions") {
            while (read_line(is, str) && !str.empty() && ((isspace(str[0]) == 0) || str[0] == '\t')) {
                int address;
                int values[4];
                if (str[0] == '\t')  // skip pretty-printed instruction text
                    continue;
                int cnt = sscanf(str.c_str(), "%d:%d%d%d%d", &address, &values[0], &values[1], &values[2], &values[3]);
                if (cnt < 2)
                    throw invalid_format("In instruction section");
                for (int i = 0; i < cnt - 1; ++i)
                    instructions.push_back(values[i]);
            }
        } else if (section == "processes") {
            while (read_line(is, str) && !str.empty() && (isspace(str[0]) == 0)) {
                auto process = process_t{};
                if (sscanf(str.c_str(), "%d:%d:%31s", &index, &process.initial, name) != 3)
                    throw invalid_format("In process section");
                process.name = name;
                this->processes.push_back(process);
            }
        } else if (section == "locations") {
            while (read_line(is, str) && !str.empty() && (isspace(str[0]) == 0)) {
                if (sscanf(str.c_str(), "%d:%d:%d", &index, &process, &invariant) != 3)
                    throw invalid_format("In location section");
                assert(index < layout.size());
                auto& cell = this->layout[index];
                assert(std::holds_alternative<cell_t::location_t>(cell.data));
                auto& location = std::get<cell_t::location_t>(cell.data);
                location.process = process;
                location.invariant = invariant;
                assert(0 <= process);
                assert(process <= processes.size());
                this->processes[process].locations.push_back(index);
            }
        } else if (section == "edges") {
            while (read_line(is, str) && !str.empty() && (isspace(str[0]) == 0)) {
                if (sscanf(str.c_str(), "%d:%d:%d:%d:%d:%d", &process, &source, &target, &guard, &sync, &update) != 6)
                    throw invalid_format("In edge section");
                assert(0 <= process);
                assert(process <= processes.size());
                this->processes[process].edges.push_back(this->edges.size());
                this->edges.push_back(edge_t{process, source, target, guard, sync, update});
            }
        } else if (section == "expressions") {
            while (read_line(is, str) && !str.empty() && (isspace(str[0]) == 0)) {
                if (sscanf(str.c_str(), "%d", &index) != 1)
                    throw invalid_format("In expression section");

                // Find expression string (after the third colon).
                auto pos = str.find_first_of(':');
                auto count = 0u;
                while (pos != str.npos && ++count < 3)
                    pos = str.find_first_of(':', pos + 1);
                if (pos == str.npos || count != 3)
                    throw invalid_format("Missing colon in expression section");

                // Trim white space.
                pos = str.find_first_not_of(" \r\n\t\v", pos + 1);
                auto end = str.find_last_not_of(" \r\n\t\v");
                this->expressions[index] = str.substr(pos, end - pos + 1);
            }
        } else {
            throw invalid_format("Unknown section");
        }
    }
    return is;
}

void State::set_bound(size_t clock_count, int i, int j, bound_t bound)
{
    assert(0 < i || 0 < j || (bound.value == 0 && bound.strict == false));
    assert(i < clock_count);
    assert(j < clock_count);
    const auto index = i * clock_count + j;
    assert(index < dbm.size());
    dbm[index] = bound;
}

const bound_t& State::get_bound(size_t clock_count, int i, int j) const
{
    assert(i < clock_count);
    assert(j < clock_count);
    const auto index = i * clock_count + j;
    assert(index < dbm.size());
    return dbm[index];
}

std::istream& State::read(const model_t& model, std::istream& is)
{
    // Read locations:
    locations.assign(model.processes.size(), -1);
    for (auto& l : locations)
        is >> l;
    is >> read_dot;

    // Read DBM: list of bounds of arbitrary length
    const auto clock_count = model.clocks.size();
    dbm.assign(clock_count * clock_count, infinity);
    for (int i = 0; i < clock_count; ++i) {
        set_bound(clock_count, 0, i, zero);
        set_bound(clock_count, i, i, zero);
    }
    int i, j, bnd;
    while (is >> i >> j >> bnd)
        if (is >> read_dot)
            set_bound(clock_count, i, j, {bnd >> 1, ((bnd & 1) != 0)});
    is.clear();  // failed to read a bound -- end of list
    is >> read_dot;

    // Read integer variable values:
    integers.assign(model.integers.size(), -1);
    for (auto& i : integers)
        is >> i;
    return is >> read_dot;
}

/** Output operator for a symbolic state. Prints the location vector,
 * the integers and the zone of the symbolic state.
 */
std::ostream& State::print(const model_t& model, std::ostream& os) const
{
    // Print location vector.
    assert(model.processes.size() == locations.size());
    for (size_t p = 0; p < model.processes.size(); ++p) {
        const auto& proc = model.processes[p];
        int idx = proc.locations[locations[p]];
        os << proc.name << '.' << model.layout[idx].name << " ";
    }

    // Print integers.
    assert(model.integers.size() == integers.size());
    for (size_t v = 0; v < model.integers.size(); ++v)
        os << model.integers[v] << "=" << integers[v] << ' ';

    // Print clocks.
    const auto clock_count = model.clocks.size();
    assert(dbm.size() == clock_count * clock_count);
    for (size_t i = 0; i < clock_count; i++) {
        for (size_t j = 0; j < clock_count; j++) {
            if (i != j) {
                const bound_t& bnd = get_bound(clock_count, i, j);
                if (bnd.value != infinity.value)
                    os << model.clocks[i] << "-" << model.clocks[j] << (bnd.strict ? "<" : "<=") << bnd.value << " ";
            }
        }
    }

    return os;
}

std::istream& Transition::read(const model_t& model, std::istream& is)
{
    edges.clear();
    int process, edge, select;
    while (is >> process >> edge) {
        auto e = Edge{process, edge};
        is >> skip_spaces;
        while (is.peek() != '\n' && is.peek() != ';') {
            if (is >> select)
                e.select.push_back(select);
            else
                throw invalid_format{"In transition select values"};
            is >> skip_spaces;
        }
        if (is.get() == '\n')  // old format without ';'
            --e.edge;          // old format indexes edges from 1, hence convert to 0-base
        edges.push_back(std::move(e));
    }
    is.clear();
    return is >> read_dot;
}

/** Prints all edges in the transition including the source, destination, guard,
 * synchronisation and assignment. */
std::ostream& Transition::print(const model_t& model, std::ostream& os) const
{
    for (const auto& edge : edges) {
        const auto& p = model.processes[edge.process];
        int eid = p.edges[edge.edge];
        const auto& e = model.edges[eid];
        int src = e.source;
        int dst = e.target;
        int guard = e.guard;
        int sync = e.sync;
        int update = e.update;
        os << p.name << '.' << model.layout[src].name << " -> " << p.name << '.' << model.layout[dst].name;
        if (!edge.select.empty()) {
            auto s = edge.select.begin(), se = edge.select.end();
            os << " [" << *s;
            while (++s != se)
                os << "," << *s;
            os << "]";
        }
        os << " {" << model.expressions.at(guard) << "; " << model.expressions.at(sync) << "; "
           << model.expressions.at(update) << ";} ";
    }

    return os;
}

std::istream& trace_t::read(const model_t& model, std::istream& is)
{
    steps.clear();
    initial.read(model, is);
    for (;;) {
        // Skip white space.
        is >> skip_spaces;

        // A dot terminates the trace.
        if (is.peek() == '.') {
            is.get();
            break;
        }

        // Read a state and a transition.
        auto state = State{};
        state.read(model, is);
        auto transition = Transition{};
        transition.read(model, is);
        steps.emplace_back(std::move(transition), std::move(state));
    }
    return is;
}

std::ostream& trace_t::print(const model_t& model, std::ostream& os) const
{
    os << "State: ";
    initial.print(model, os) << endl;
    for (const auto& step : steps) {
        step.transition.print(model, os << "\nTransition: ") << endl;
        step.state.print(model, os << "\nState: ") << endl;
    }
    return os;
}

int main(int argc, char* argv[])
{
    try {
        if (argc < 3) {
            std::cerr << "Utility reads a model (intermediate format) and a trace (xtr/\"dot\" format) and produces "
                         "human readable trace.";
            std::cerr << "Synopsis:\n\t" << argv[0] << " <if-file> <xtr-trace-file>\n";
            std::exit(EXIT_FAILURE);
        }
        auto model = model_t{};
        // Load model in intermediate format.
        if (strcmp(argv[1], "-") == 0)
            model.read(std::cin);
        else {
            auto file = std::ifstream{argv[1]};
            if (file.fail()) {
                perror(argv[1]);
                std::exit(EXIT_FAILURE);
            }
            model.read(file);
        }

        // Load trace.
        auto file = std::ifstream{argv[2]};
        if (file.fail()) {
            perror(argv[2]);
            std::exit(EXIT_FAILURE);
        }
        auto trace = trace_t{};
        trace.read(model, file);
        trace.print(model, std::cout);
    } catch (std::exception& e) {
        std::cerr << "Caught exception: " << e.what() << endl;
        std::exit(EXIT_FAILURE);
    }
}
