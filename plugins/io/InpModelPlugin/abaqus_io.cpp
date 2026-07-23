#include "abaqus_io.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

static std::unordered_map<std::string, std::string> abaqus_to_meshio_type = {
    // trusses
    { "T2D2", "line" },
    { "T2D2H", "line" },
    { "T2D3", "line3" },
    { "T2D3H", "line3" },
    { "T3D2", "line" },
    { "T3D2H", "line" },
    { "T3D3", "line3" },
    { "T3D3H", "line3" },
    // beams
    { "B21", "line" },
    { "B21H", "line" },
    { "B22", "line3" },
    { "B22H", "line3" },
    { "B31", "line" },
    { "B31H", "line" },
    { "B32", "line3" },
    { "B32H", "line3" },
    { "B33", "line3" },
    { "B33H", "line3" },
    // surfaces
    { "CPS4", "quad" },
    { "CPS4R", "quad" },
    { "S4", "quad" },
    { "S4R", "quad" },
    { "S4RS", "quad" },
    { "S4RSW", "quad" },
    { "S4R5", "quad" },
    { "S8R", "quad8" },
    { "S8R5", "quad8" },
    { "S9R5", "quad9" },
    { "CPS3", "triangle" },
    { "STRI3", "triangle" },
    { "S3", "triangle" },
    { "S3R", "triangle" },
    { "S3RS", "triangle" },
    { "R3D3", "triangle" },
    { "STRI65", "triangle6" },
    // volumes
    { "C3D8", "hexahedron" },
    { "C3D8H", "hexahedron" },
    { "C3D8I", "hexahedron" },
    { "C3D8IH", "hexahedron" },
    { "C3D8R", "hexahedron" },
    { "C3D8RH", "hexahedron" },
    { "C3D20", "hexahedron20" },
    { "C3D20H", "hexahedron20" },
    { "C3D20R", "hexahedron20" },
    { "C3D20RH", "hexahedron20" },
    { "C3D4", "tetra" },
    { "C3D4H", "tetra4" },
    { "C3D10", "tetra10" },
    { "C3D10H", "tetra10" },
    { "C3D10I", "tetra10" },
    { "C3D10M", "tetra10" },
    { "C3D10MH", "tetra10" },
    { "C3D6", "wedge" },
    { "C3D15", "wedge15" },
    // misc
    { "CAX4P", "quad" },
    { "CPE6", "triangle6" },
};

// map from meshio cell type -> number of nodes (used for strict validation)
static std::unordered_map<std::string, int> num_nodes_per_cell = {
    { "line", 2 },
    { "line3", 3 },
    { "triangle", 3 },
    { "triangle6", 6 },
    { "quad", 4 },
    { "quad8", 8 },
    { "quad9", 9 },
    { "hexahedron", 8 },
    { "hexahedron20", 20 },
    { "tetra", 4 },
    { "tetra4", 4 },
    { "tetra10", 10 },
    { "wedge", 6 },
    { "wedge15", 15 },
};

static std::unordered_map<std::string, std::string> meshio_to_abaqus_type; // filled on first use

static std::string upper_copy(const std::string& s)
{
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::toupper(c); });
    return r;
}

static std::vector<std::string> split_commas(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

static std::unordered_map<std::string, std::string> get_param_map(const std::string& word, const std::vector<std::string>& required_keys = {})
{
    // remove leading '*' if present
    std::string w = word;
    if (!w.empty() && w[0] == '*')
        w = w.substr(1);
    auto parts = split_commas(w);
    std::unordered_map<std::string, std::string> param_map;
    for (auto& p : parts) {
        std::string t = p;
        // trim
        auto l = t.find_first_not_of(" \t\r\n");
        auto r = t.find_last_not_of(" \t\r\n");
        if (l == std::string::npos)
            continue;
        t = t.substr(l, r - l + 1);
        auto eq = t.find('=');
        if (eq == std::string::npos) {
            param_map[upper_copy(t)] = "";
        } else {
            std::string key = upper_copy(t.substr(0, eq));
            std::string val = t.substr(eq + 1);
            param_map[key] = val;
        }
    }
    for (auto& k : required_keys) {
        if (param_map.find(k) == param_map.end()) {
            throw std::runtime_error(k + " not found in " + word);
        }
    }
    return param_map;
}

// forward
static Mesh_meshIO read_buffer(std::istream& in, const fs::path& current_file);

Mesh_meshIO read_abaqus(const std::string& filename)
{
    if (meshio_to_abaqus_type.empty()) {
        for (auto& kv : abaqus_to_meshio_type) {
            meshio_to_abaqus_type[kv.second] = kv.first;
        }
    }
    std::ifstream f(filename);
    if (!f)
        throw std::runtime_error("Cannot open file: " + filename);
    fs::path p = filename;
    return read_buffer(f, p);
}

static std::string readline_trimmed(std::istream& in)
{
    std::string line;
    if (!std::getline(in, line))
        return std::string();
    // remove possible \r
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return line;
}

static Mesh_meshIO read_buffer(std::istream& in, const fs::path& current_file)
{
    Mesh_meshIO mesh;
    std::vector<std::vector<double>> points;
    std::vector<CellBlock> cells;
    std::vector<std::unordered_map<int, int>> cell_ids; // per block: orig id -> local idx

    std::unordered_map<std::string, std::vector<int>> point_sets;
    std::unordered_map<std::string, std::vector<std::vector<int>>> cell_sets;
    std::unordered_map<std::string, std::vector<int>> cell_sets_element;
    std::vector<std::string> cell_sets_element_order;

    std::string line = readline_trimmed(in);
    std::unordered_map<int, int> point_id_map; // orig id -> new index

    while (!line.empty()) {
        if (line.rfind("**", 0) == 0) { // comment
            line = readline_trimmed(in);
            continue;
        }
        // keyword is before first comma
        auto part = line;
        auto pos = part.find(',');
        std::string key = (pos == std::string::npos) ? part : part.substr(0, pos);
        // strip leading * and whitespace
        key.erase(0, key.find_first_not_of("* \t\r\n"));
        key = upper_copy(key);
        if (key == "NODE") {
            // read nodes
            if (points.empty()) {
                points.reserve(1024);
                point_id_map.reserve(1024);
            }
            while (true) {
                std::string l = readline_trimmed(in);
                if (l.empty() || l[0] == '*') {
                    line = l;
                    break;
                }
                // skip blank
                if (std::all_of(l.begin(), l.end(), [](char c) { return std::isspace((unsigned char)c); })) {
                    continue;
                }
                // parse id, coords
                // allow spaces after commas
                std::stringstream ss(l);
                std::vector<std::string> toks;
                std::string tok;
                while (std::getline(ss, tok, ',')) {
                    // trim
                    auto a = tok.find_first_not_of(" \t\r\n");
                    if (a == std::string::npos)
                        toks.push_back("");
                    else {
                        auto b = tok.find_last_not_of(" \t\r\n");
                        toks.push_back(tok.substr(a, b - a + 1));
                    }
                }
                if (toks.size() < 2)
                    continue;
                int orig_id = std::stoi(toks[0]);
                std::vector<double> coords;
                coords.reserve(toks.size() - 1);
                for (std::size_t i = 1; i < toks.size(); ++i)
                    coords.push_back(std::stod(toks[i]));
                point_id_map[orig_id] = (int)points.size();
                points.push_back(coords);
            }
        } else if (key == "ELEMENT") {
            if (point_id_map.empty())
                throw std::runtime_error("Expected NODE before ELEMENT");
            auto params = get_param_map(line, { "TYPE" });
            std::string etype = params["TYPE"];
            if (abaqus_to_meshio_type.find(etype) == abaqus_to_meshio_type.end()) {
                // unknown element type: suggest adding to mapping
                throw std::runtime_error("Element type not available: " + etype + ". Consider adding it to abaqus_to_meshio_type.");
            }
            std::string cell_type = abaqus_to_meshio_type[etype];
            // determine expected nodes per cell from table
            if (num_nodes_per_cell.find(cell_type) == num_nodes_per_cell.end()) {
                throw std::runtime_error("Unknown node count for meshio cell type: " + cell_type);
            }
            int nodes_per_cell = num_nodes_per_cell[cell_type];
            int num_data = nodes_per_cell + 1; // element id + node ids

            // parse lines, accumulate tokens and extract elements when enough tokens
            std::vector<int> token_buf;
            token_buf.reserve(static_cast<size_t>(num_data) * 64);
            std::string l;
            std::vector<std::vector<int>> elems;
            elems.reserve(512);
            std::unordered_map<int, int> local_ids;
            local_ids.reserve(512);
            int counter = 0;
            while (true) {
                l = readline_trimmed(in);
                if (l.empty() || l[0] == '*') {
                    line = l;
                    break;
                }
                if (std::all_of(l.begin(), l.end(), [](char c) { return std::isspace((unsigned char)c); }))
                    continue;
                std::stringstream ss(l);
                std::string t;
                while (std::getline(ss, t, ',')) {
                    auto a = t.find_first_not_of(" \t\r\n");
                    if (a == std::string::npos)
                        continue;
                    auto b = t.find_last_not_of(" \t\r\n");
                    std::string tok = t.substr(a, b - a + 1);
                    if (tok.empty())
                        continue;
                    try {
                        int v = std::stoi(tok);
                        token_buf.push_back(v);
                    } catch (...) {
                        // ignore non-int tokens silently
                        continue;
                    }
                    // while we have enough tokens for an element, extract
                    while ((int)token_buf.size() >= num_data) {
                        std::vector<int> nodes;
                        nodes.reserve(nodes_per_cell);
                        int orig_eid = token_buf[0];
                        for (int k = 1; k <= nodes_per_cell; ++k)
                            nodes.push_back(token_buf[k]);
                        // remove first num_data tokens
                        token_buf.erase(token_buf.begin(), token_buf.begin() + num_data);
                        // map node ids to internal indices
                        std::vector<int> mapped;
                        mapped.reserve(nodes.size());
                        for (int orig_nid : nodes) {
                            auto it = point_id_map.find(orig_nid);
                            if (it == point_id_map.end()) {
                                throw std::runtime_error("Unknown node id in ELEMENT: " + std::to_string(orig_nid));
                            }
                            mapped.push_back(it->second);
                        }
                        local_ids[orig_eid] = counter++;
                        elems.push_back(std::move(mapped));
                    }
                }
            }
            if (elems.empty() && token_buf.empty()) {
                // no elements
                cells.push_back(CellBlock { cell_type, {} });
                cell_ids.push_back({});
                continue;
            }
            CellBlock cb;
            cb.type = cell_type;
            cb.data = elems;
            cb.abaqus_type = etype;
            cells.push_back(cb);
            cell_ids.push_back(local_ids);
            std::unordered_map<int, int> empty_map;
            if (params.find("ELSET") != params.end()) {
                std::string nm = params["ELSET"];
                int n_elems = elems.size();
                std::vector<int> all_idx(n_elems);
                for (int i = 0; i < n_elems; ++i)
                    all_idx[i] = i;
                cell_sets_element[nm] = all_idx;
                cell_sets_element_order.push_back(nm);
            }
        } else if (key == "NSET") {
            auto params = get_param_map(line, { "NSET" });
            std::string name = params["NSET"];
            std::vector<int> set_ids;
            std::string l;
            while (true) {
                l = readline_trimmed(in);
                if (l.empty() || l[0] == '*') {
                    line = l;
                    break;
                }
                if (std::all_of(l.begin(), l.end(), [](char c) { return std::isspace((unsigned char)c); }))
                    continue;
                std::stringstream ss(l);
                std::string t;
                while (std::getline(ss, t, ',')) {
                    auto a = t.find_first_not_of(" \t\r\n");
                    if (a == std::string::npos)
                        continue;
                    auto b = t.find_last_not_of(" \t\r\n");
                    std::string tok = t.substr(a, b - a + 1);
                    if (!tok.empty()) {
                        try {
                            set_ids.push_back(std::stoi(tok));
                        } catch (...) {
                            spdlog::warn("Skipping non-integer token in NSET: '{}'", tok);
                        }
                    }
                }
            }
            std::vector<int> mapped;
            for (int sid : set_ids) {
                auto it = point_id_map.find(sid);
                if (it == point_id_map.end()) {
                    spdlog::warn("NSET references unknown node id {} - skipping", sid);
                    continue;
                }
                mapped.push_back(it->second);
            }
            point_sets[name] = mapped;
        } else if (key == "ELSET") {
            auto params = get_param_map(line, { "ELSET" });
            std::string name = params["ELSET"];
            std::vector<int> set_ids;
            std::vector<std::string> set_names;
            std::string l;
            while (true) {
                l = readline_trimmed(in);
                if (l.empty() || l[0] == '*') {
                    line = l;
                    break;
                }
                if (std::all_of(l.begin(), l.end(), [](char c) { return std::isspace((unsigned char)c); }))
                    continue;
                std::stringstream ss(l);
                std::string t;
                while (std::getline(ss, t, ',')) {
                    auto a = t.find_first_not_of(" \t\r\n");
                    if (a == std::string::npos)
                        continue;
                    auto b = t.find_last_not_of(" \t\r\n");
                    std::string tok = t.substr(a, b - a + 1);
                    if (!tok.empty()) {
                        // numeric?
                        bool isnumeric = true;
                        for (char c : tok)
                            if (!std::isdigit((unsigned char)c)) {
                                isnumeric = false;
                                break;
                            }
                        if (isnumeric)
                            set_ids.push_back(std::stoi(tok));
                        else
                            set_names.push_back(tok);
                    }
                }
            }
            cell_sets[name] = std::vector<std::vector<int>>();
            if (!set_ids.empty()) {
                // for each existing cell block, produce the intersection
                for (std::size_t ic = 0; ic < cell_ids.size(); ++ic) {
                    std::vector<int> out;
                    for (int sid : set_ids) {
                        auto it = cell_ids[ic].find(sid);
                        if (it != cell_ids[ic].end())
                            out.push_back(it->second);
                    }
                    cell_sets[name].push_back(out);
                }
            } else if (!set_names.empty()) {
                for (auto& sn : set_names) {
                    if (cell_sets.find(sn) != cell_sets.end()) {
                        // append existing
                        cell_sets[name].push_back(std::vector<int>());
                    } else if (cell_sets_element.find(sn) != cell_sets_element.end()) {
                        // append element-level
                        // will be fixed later
                        cell_sets[name].push_back(cell_sets_element[sn]);
                    } else {
                        throw std::runtime_error(std::string("Unknown cell set '") + sn + "'");
                    }
                }
            }
        } else if (key == "INCLUDE") {
            // parse INPUT=...
            auto pos = line.find('=');
            if (pos == std::string::npos) {
                line = readline_trimmed(in);
                continue;
            }
            std::string path = line.substr(pos + 1);
            // trim
            auto a = path.find_first_not_of(" \t\r\n");
            auto b = path.find_last_not_of(" \t\r\n");
            if (a == std::string::npos) {
                line = readline_trimmed(in);
                continue;
            }
            path = path.substr(a, b - a + 1);
            if (path.find("..") != std::string::npos) {
                spdlog::error("Relative paths with .. are not allowed in INCLUDE");
                line = readline_trimmed(in);
                continue;
            }
            fs::path ext = path;
            if (!ext.is_absolute())
                ext = current_file.parent_path() / ext;
            if (!fs::exists(ext)) {
                line = readline_trimmed(in);
                continue;
            }
            std::ifstream ef(ext);
            if (ef) {
                Mesh_meshIO out = read_buffer(ef, ext);
                // merge points and cells into current containers
                int new_point_id = (int)points.size();
                for (auto& p : out.points)
                    points.push_back(p);
                for (auto& cb : out.cells) {
                    CellBlock nb;
                    nb.type = cb.type;
                    nb.abaqus_type = cb.abaqus_type;
                    nb.data.reserve(cb.data.size());
                    for (auto& row : cb.data) {
                        std::vector<int> r2;
                        r2.reserve(row.size());
                        for (int nid : row)
                            r2.push_back(nid + new_point_id);
                        nb.data.push_back(r2);
                    }
                    cells.push_back(nb);
                }
                // merge point_sets, shifting indices
                for (auto& kv : out.point_sets) {
                    std::vector<int> shifted;
                    shifted.reserve(kv.second.size());
                    for (int v : kv.second)
                        shifted.push_back(v + new_point_id);
                    // if key exists, append; otherwise set
                    if (point_sets.find(kv.first) != point_sets.end()) {
                        auto& existing = point_sets[kv.first];
                        existing.insert(existing.end(), shifted.begin(), shifted.end());
                    } else {
                        point_sets[kv.first] = shifted;
                    }
                }
                // TODO: merge cell_sets and other metadata if necessary
            } else {
                spdlog::warn("INCLUDE file exists but could not be opened: {}", ext.string());
            }
            line = readline_trimmed(in);
        } else {
            // skip unknown keywords
            line = readline_trimmed(in);
        }
    }

    // post-process cell_sets_element
    for (std::size_t i = 0; i < cell_sets_element_order.size(); ++i) {
        auto& name = cell_sets_element_order[i];
        if (cell_sets.find(name) != cell_sets.end()) {
            // replace i-th entry
            if (i < cell_sets[name].size())
                cell_sets[name][i] = cell_sets_element[name];
        } else {
            cell_sets[name] = std::vector<std::vector<int>>();
            for (std::size_t ic = 0; ic < cells.size(); ++ic) {
                if (i == ic)
                    cell_sets[name].push_back(cell_sets_element[name]);
                else
                    cell_sets[name].push_back(std::vector<int>());
            }
        }
    }

    mesh.points = std::move(points);
    mesh.cells = std::move(cells);
    mesh.point_sets = std::move(point_sets);
    mesh.cell_sets = std::move(cell_sets);
    return mesh;
}

void write_abaqus(const std::string& filename, const Mesh_meshIO& mesh, const std::string& float_fmt, bool translate_cell_names)
{
    std::ofstream f(filename);
    if (!f)
        throw std::runtime_error("Cannot open for writing: " + filename);
    f << "*HEADING\n";
    f << "Abaqus DataFile generated by C++ converter\n";
    f << "*NODE\n";
    // write nodes
    for (std::size_t k = 0; k < mesh.points.size(); ++k) {
        f << (k + 1);
        for (double x : mesh.points[k]) {
            char buf[128];
            // float_fmt is expected to be a printf-style format like "%.16e" or "%.6f"
            // guard: ensure format contains a single % specifier
            try {
                std::snprintf(buf, sizeof(buf), float_fmt.c_str(), x);
                f << "," << buf;
            } catch (...) {
                // fallback
                f << "," << x;
            }
        }
        f << '\n';
    }
    int eid = 0;
    for (auto& cell_block : mesh.cells) {
        std::string cell_type = cell_block.type;
        std::string name = cell_type;
        // prefer original Abaqus type if present
        if (translate_cell_names && !cell_block.abaqus_type.empty()) {
            name = cell_block.abaqus_type;
        } else if (translate_cell_names) {
            auto it = meshio_to_abaqus_type.find(cell_type);
            if (it != meshio_to_abaqus_type.end())
                name = it->second;
        }
        f << "*ELEMENT, TYPE=" << name << "\n";
        for (auto& row : cell_block.data) {
            ++eid;
            f << eid;
            for (int nid : row)
                f << "," << (nid + 1);
            f << '\n';
        }
    }
    const int nnl = 8;
    int offset = 0;
    for (std::size_t ic = 0; ic < mesh.cells.size(); ++ic) {
        for (auto& kv : mesh.cell_sets) {
            auto& k = kv.first;
            auto& v = kv.second;
            if (ic < v.size() && !v[ic].empty()) {
                std::vector<std::string> els;
                els.reserve(v[ic].size());
                for (int i : v[ic])
                    els.push_back(std::to_string(i + 1 + offset));
                f << "*ELSET, ELSET=" << k << "\n";
                for (std::size_t i = 0; i < els.size(); i += nnl) {
                    for (std::size_t j = i; j < std::min(els.size(), i + nnl); ++j) {
                        if (j > i)
                            f << ",";
                        f << els[j];
                    }
                    f << "\n";
                }
            }
        }
        offset += (int)mesh.cells[ic].data.size();
    }
    for (auto& kv : mesh.point_sets) {
        auto& k = kv.first;
        auto& v = kv.second;
        std::vector<std::string> nds;
        nds.reserve(v.size());
        for (int i : v)
            nds.push_back(std::to_string(i + 1));
        f << "*NSET, NSET=" << k << "\n";
        for (std::size_t i = 0; i < nds.size(); i += nnl) {
            for (std::size_t j = i; j < std::min(nds.size(), i + nnl); ++j) {
                if (j > i)
                    f << ",";
                f << nds[j];
            }
            f << "\n";
        }
    }
}