#include "ZaxeArchive.hpp"

namespace Slic3r {

namespace {
std::string to_json(const ConfMap &m)
{
    ptree pt; // construct root obj.
    stringstream ss;
    // replace string numbers with numbers. better than translator solution imho.
    std::regex reg("\\\"(-?[0-9]+\\.{0,1}[0-9]*)\\\"");

    for (auto &param : m) pt.put(param.first, param.second);

    json_parser::write_json(ss, pt);

    return std::regex_replace(ss.str(), reg, "$1");
}

std::string get_cfg_value(const DynamicPrintConfig &cfg, const std::string &key, const std::string &default_val = "")
{
    std::string ret = default_val; // start with default.
    if (cfg.has(key)) {
        auto opt = cfg.option(key);
        if (opt) ret = opt->serialize();
        if (ret.empty()) ret = default_val; // again to default when empty.
    }
    return ret;
}

std::string generate_md5_checksum(const string file_path)
{
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    char buffer[4096];
    ifstream file(file_path.c_str(), ifstream::binary);

    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        MD5_Update(&md5Context, buffer, file.gcount());
    }

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);

    stringstream md5string;
    md5string << hex << uppercase << setfill('0');
    for (const auto &byte: result)
        md5string << setw(2) << (int)byte;

    return to_lower_copy(md5string.str());
}

ifstream::pos_type read_binary_into_buffer(const char* path, vector<char>& bytes)
{
    ifstream ifs(path, ios::in | ios::binary | ios::ate);
    ifstream::pos_type fileSize = ifs.tellg(); // get the file size.
    ifs.seekg(0, ios::beg); // seek to beginning.
    bytes.resize(fileSize); // resize.
    ifs.read(bytes.data(), fileSize); // read.
    return fileSize;
}

static void write_thumbnail(Zipper &zipper, const ThumbnailData &data)
{
    size_t png_size = 0;

    void *png_data = tdefl_write_image_to_png_file_in_memory_ex(
         (const void *) data.pixels.data(), data.width, data.height, 4,
         &png_size, MZ_DEFAULT_LEVEL, 1);

    if (png_data != nullptr) {
        zipper.add_entry("snapshot.png", static_cast<const std::uint8_t *>(png_data), png_size);

        mz_free(png_data);
    }
}
} // namespace

std::string ZaxeArchive::get_info(const string &key) const
{
    auto it = m_infoconf.find(key);
    if (it != m_infoconf.end())
        return it->second;
    return "";
}
void ZaxeArchive::export_print(const string archive_path, ThumbnailsList thumbnails, const Print &print, const string temp_gcode_output_path)
{
    Zipper zipper{archive_path};
    boost::filesystem::path temp_path(temp_gcode_output_path);
    vector<char> bytes;
    std::string gcode;

    try {
        m_infoconf = ConfMap{
            { "name", boost::filesystem::path(zipper.get_filename()).stem().string() },
            { "checksum", generate_md5_checksum(temp_gcode_output_path)
        } };
        generate_info_file(m_infoconf, print); // generate info.json contents.
        zipper.add_entry("info.json");
        zipper << to_json(m_infoconf);
        // add gcode
        // load file into string again. FIXME maybe a better way to get the string beforehand.
        boost::filesystem::load_string_file(temp_gcode_output_path, gcode);
        zipper.add_entry("data.zaxe_code");
        zipper << gcode;
        // add model stl
        string model_path = (temp_path.parent_path() / "model.stl").string();
        ifstream::pos_type fileSize = read_binary_into_buffer(model_path.c_str(), bytes);
        zipper.add_entry("model.stl", bytes.data(), fileSize);
        for (const ThumbnailData& data : thumbnails)
            if (data.is_valid()) write_thumbnail(zipper, data);
        //BOOST_LOG_TRIVIAL(debug) << to_json(m_infoconf);
        zipper.finalize();
        BOOST_LOG_TRIVIAL(info) << "Zaxe file generated successfully to: " << zipper.get_filename();
    } catch(std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
        throw; // Rethrow the exception
    }
}

void ZaxeArchive::generate_info_file(ConfMap &m, const Print &print)
{
    auto &cfg = print.full_print_config();
    PrintStatistics stats = print.print_statistics();

    // standby temp calculation
    float temp = stof(get_cfg_value(cfg, "temperature"));
    float standby_temp_delta = stof(get_cfg_value(cfg, "standby_temperature_delta"));
    char standby_temp_char[100];
    sprintf(standby_temp_char, "%.2f", (temp + standby_temp_delta));
    string fill_density = get_cfg_value(cfg, "fill_density");
    bool has_raft = stoi(get_cfg_value(cfg, "raft_layers")) > 0;
    bool has_skirt = stoi(get_cfg_value(cfg, "skirts")) > 0;

    m["version"]                = ZAXE_FILE_VERSION;
    m["duration"]               = get_time_hms(stats.estimated_normal_print_time);
    m["raft"]                   = has_raft ? "raft" : (has_skirt ? "skirt" : "none");
    m["layer_height"]           = get_cfg_value(cfg, "layer_height");
    m["infill_density"]         = fill_density.replace(fill_density.find("%"), 1, "");
    m["support_angle"]          = get_cfg_value(cfg, "support_material_angle");
    m["material"]               = get_cfg_value(cfg, "filament_notes"); // FIXME change this to filament code later.
    m["model"]                  = get_cfg_value(cfg, "printer_model");
    m["printer_profile"]        = get_cfg_value(cfg, "printer_settings_id");
    m["filament_used"]          = std::to_string(stats.total_used_filament);
    m["nozzle_diameter"]        = get_cfg_value(cfg, "printer_variant", "0.4");
    m["extruder_temperature"]   = get_cfg_value(cfg, "first_layer_temperature");
    m["bed_temperature"]        = get_cfg_value(cfg, "bed_temperature");
    m["standby_temperature"]    = standby_temp_char;
    m["slicer_version"]         = SLIC3R_BUILD_ID;
}

} // namespace Slic3r
