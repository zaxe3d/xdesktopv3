#include "Calib.hpp"
#include "Config.hpp"
#include "Model.hpp"
#include "GCode.hpp"
#include <cmath>

namespace Slic3r {
std::string CalibPressureAdvance::move_to(Vec2d        pt,
                                          GCodeWriter &writer,
                                          std::string  comment)
{
    std::stringstream gcode;

    gcode << writer.retract();
    gcode << writer.travel_to_xy(pt, comment);
    gcode << writer.unretract();

    m_last_pos = Vec3d(pt.x(), pt.y(), 0);

    return gcode.str();
}

double CalibPressureAdvance::e_per_mm(double line_width,
                                      double layer_height,
                                      float  nozzle_diameter,
                                      float  filament_diameter,
                                      float  print_flow_ratio) const
{
    const Flow   line_flow = Flow(line_width, layer_height, nozzle_diameter);
    const double filament_area = M_PI * std::pow(filament_diameter / 2, 2);

    return line_flow.mm3_per_mm() / filament_area * print_flow_ratio;
}

std::string CalibPressureAdvance::convert_number_to_string(double num) const
{
    auto sNumber = std::to_string(num);
    sNumber.erase(sNumber.find_last_not_of('0') + 1, std::string::npos);
    sNumber.erase(sNumber.find_last_not_of('.') + 1, std::string::npos);

    return sNumber;
}

std::string CalibPressureAdvance::draw_digit(
    double                              startx,
    double                              starty,
    char                                c,
    CalibPressureAdvance::DrawDigitMode mode,
    double                              line_width,
    double                              e_per_mm,
    GCodeWriter                        &writer)
{
    const double len = m_digit_segment_len;
    const double gap = line_width / 2.0;

    const auto dE     = e_per_mm * len;
    const auto two_dE = dE * 2;

    Vec2d p0, p1, p2, p3, p4, p5;
    Vec2d p0_5, p4_5;
    Vec2d gap_p0_toward_p3, gap_p2_toward_p3;
    Vec2d dot_direction;

    if (mode == CalibPressureAdvance::DrawDigitMode::Bottom_To_Top) {
        //  1-------2-------5
        //  |       |       |
        //  |       |       |
        //  0-------3-------4
        p0   = Vec2d(startx, starty);
        p0_5 = Vec2d(startx, starty + len / 2);
        p1   = Vec2d(startx, starty + len);
        p2   = Vec2d(startx + len, starty + len);
        p3   = Vec2d(startx + len, starty);
        p4   = Vec2d(startx + len * 2, starty);
        p4_5 = Vec2d(startx + len * 2, starty + len / 2);
        p5   = Vec2d(startx + len * 2, starty + len);

        gap_p0_toward_p3 = p0 + Vec2d(gap, 0);
        gap_p2_toward_p3 = p2 + Vec2d(0, gap);

        dot_direction = Vec2d(-len / 2, 0);
    } else {
        //  0-------1
        //  |       |
        //  3-------2
        //  |       |
        //  4-------5
        p0   = Vec2d(startx, starty);
        p0_5 = Vec2d(startx + len / 2, starty);
        p1   = Vec2d(startx + len, starty);
        p2   = Vec2d(startx + len, starty - len);
        p3   = Vec2d(startx, starty - len);
        p4   = Vec2d(startx, starty - len * 2);
        p4_5 = Vec2d(startx + len / 2, starty - len * 2);
        p5   = Vec2d(startx + len, starty - len * 2);

        gap_p0_toward_p3 = p0 - Vec2d(0, gap);
        gap_p2_toward_p3 = p2 - Vec2d(gap, 0);

        dot_direction = Vec2d(0, len / 2);
    }

    std::stringstream gcode;

    switch (c) {
    case '0':
        gcode << move_to(p0, writer, "Glyph: 0");
        gcode << writer.extrude_to_xy(p1, dE);
        gcode << writer.extrude_to_xy(p5, two_dE);
        gcode << writer.extrude_to_xy(p4, dE);
        gcode << writer.extrude_to_xy(gap_p0_toward_p3, two_dE);
        break;
    case '1':
        gcode << move_to(p0_5, writer, "Glyph: 1");
        gcode << writer.extrude_to_xy(p4_5, two_dE);
        break;
    case '2':
        gcode << move_to(p0, writer, "Glyph: 2");
        gcode << writer.extrude_to_xy(p1, dE);
        gcode << writer.extrude_to_xy(p2, dE);
        gcode << writer.extrude_to_xy(p3, dE);
        gcode << writer.extrude_to_xy(p4, dE);
        gcode << writer.extrude_to_xy(p5, dE);
        break;
    case '3':
        gcode << move_to(p0, writer, "Glyph: 3");
        gcode << writer.extrude_to_xy(p1, dE);
        gcode << writer.extrude_to_xy(p5, two_dE);
        gcode << writer.extrude_to_xy(p4, dE);
        gcode << move_to(gap_p2_toward_p3, writer);
        gcode << writer.extrude_to_xy(p3, dE);
        break;
    case '4':
        gcode << move_to(p0, writer, "Glyph: 4");
        gcode << writer.extrude_to_xy(p3, dE);
        gcode << writer.extrude_to_xy(p2, dE);
        gcode << move_to(p1, writer);
        gcode << writer.extrude_to_xy(p5, two_dE);
        break;
    case '5':
        gcode << move_to(p1, writer, "Glyph: 5");
        gcode << writer.extrude_to_xy(p0, dE);
        gcode << writer.extrude_to_xy(p3, dE);
        gcode << writer.extrude_to_xy(p2, dE);
        gcode << writer.extrude_to_xy(p5, dE);
        gcode << writer.extrude_to_xy(p4, dE);
        break;
    case '6':
        gcode << move_to(p1, writer, "Glyph: 6");
        gcode << writer.extrude_to_xy(p0, dE);
        gcode << writer.extrude_to_xy(p4, two_dE);
        gcode << writer.extrude_to_xy(p5, dE);
        gcode << writer.extrude_to_xy(p2, dE);
        gcode << writer.extrude_to_xy(p3, dE);
        break;
    case '7':
        gcode << move_to(p0, writer, "Glyph: 7");
        gcode << writer.extrude_to_xy(p1, dE);
        gcode << writer.extrude_to_xy(p5, two_dE);
        break;
    case '8':
        gcode << move_to(p2, writer, "Glyph: 8");
        gcode << writer.extrude_to_xy(p3, dE);
        gcode << writer.extrude_to_xy(p4, dE);
        gcode << writer.extrude_to_xy(p5, dE);
        gcode << writer.extrude_to_xy(p1, two_dE);
        gcode << writer.extrude_to_xy(p0, dE);
        gcode << writer.extrude_to_xy(p3, dE);
        break;
    case '9':
        gcode << move_to(p5, writer, "Glyph: 9");
        gcode << writer.extrude_to_xy(p1, two_dE);
        gcode << writer.extrude_to_xy(p0, dE);
        gcode << writer.extrude_to_xy(p3, dE);
        gcode << writer.extrude_to_xy(p2, dE);
        break;
    case '.':
        gcode << move_to(p4_5, writer, "Glyph: .");
        gcode << writer.extrude_to_xy(p4_5 + dot_direction, dE);
        break;
    default: break;
    }

    return gcode.str();
}

std::string CalibPressureAdvance::draw_number(
    double                              startx,
    double                              starty,
    double                              value,
    CalibPressureAdvance::DrawDigitMode mode,
    double                              line_width,
    double                              e_per_mm,
    double                              speed,
    GCodeWriter                        &writer)
{
    auto              sNumber = convert_number_to_string(value);
    std::stringstream gcode;
    gcode << writer.set_speed(speed);

    for (std::string::size_type i = 0; i < sNumber.length(); ++i) {
        if (i > m_max_number_len) { break; }
        switch (mode) {
        case DrawDigitMode::Bottom_To_Top:
            gcode << draw_digit(startx, starty + i * number_spacing(),
                                sNumber[i], mode, line_width, e_per_mm,
                                writer);
            break;
        default:
            gcode << draw_digit(startx + i * number_spacing(), starty,
                                sNumber[i], mode, line_width, e_per_mm,
                                writer);
        }
    }

    return gcode.str();
}

std::string CalibPressureAdvance::draw_box(GCodeWriter   &writer,
                                           double         min_x,
                                           double         min_y,
                                           double         size_x,
                                           double         size_y,
                                           DrawBoxOptArgs opt_args)
{
    std::stringstream gcode;

    double       x     = min_x;
    double       y     = min_y;
    const double max_x = min_x + size_x;
    const double max_y = min_y + size_y;

    const double spacing = opt_args.line_width -
                           opt_args.height * (1 - M_PI / 4);

    // if number of perims exceeds size of box, reduce it to max
    const int max_perimeters = std::min(
        // this is the equivalent of number of perims for concentric fill
        std::floor(size_x * std::sin(to_radians(45))) /
            (spacing / std::sin(to_radians(45))),
        std::floor(size_y * std::sin(to_radians(45))) /
            (spacing / std::sin(to_radians(45))));

    opt_args.num_perimeters = std::min(opt_args.num_perimeters,
                                       max_perimeters);

    gcode << move_to(Vec2d(min_x, min_y), writer, "Move to box start");

    // DrawLineOptArgs line_opt_args(*this);
    auto        line_arg_height     = opt_args.height;
    auto        line_arg_line_width = opt_args.line_width;
    auto        line_arg_speed      = opt_args.speed;
    std::string comment             = "";

    for (int i = 0; i < opt_args.num_perimeters; ++i) {
        if (i !=
            0) { // after first perimeter, step inwards to start next perimeter
            x += spacing;
            y += spacing;
            gcode << move_to(Vec2d(x, y), writer,
                             "Step inwards to print next perimeter");
        }

        y += size_y - i * spacing * 2;
        comment = "Draw perimeter (up)";
        gcode << draw_line(writer, Vec2d(x, y), line_arg_line_width,
                           line_arg_height, line_arg_speed, comment);

        x += size_x - i * spacing * 2;
        comment = "Draw perimeter (right)";
        gcode << draw_line(writer, Vec2d(x, y), line_arg_line_width,
                           line_arg_height, line_arg_speed, comment);

        y -= size_y - i * spacing * 2;
        comment = "Draw perimeter (down)";
        gcode << draw_line(writer, Vec2d(x, y), line_arg_line_width,
                           line_arg_height, line_arg_speed, comment);

        x -= size_x - i * spacing * 2;
        comment = "Draw perimeter (left)";
        gcode << draw_line(writer, Vec2d(x, y), line_arg_line_width,
                           line_arg_height, line_arg_speed, comment);
    }

    if (!opt_args.is_filled) { return gcode.str(); }

    // create box infill
    const double spacing_45 = spacing / std::sin(to_radians(45));

    const double bound_modifier = (spacing * (opt_args.num_perimeters - 1)) +
                                  (opt_args.line_width * (1 - m_encroachment));
    const double x_min_bound = min_x + bound_modifier;
    const double x_max_bound = max_x - bound_modifier;
    const double y_min_bound = min_y + bound_modifier;
    const double y_max_bound = max_y - bound_modifier;
    const int x_count = std::floor((x_max_bound - x_min_bound) / spacing_45);
    const int y_count = std::floor((y_max_bound - y_min_bound) / spacing_45);

    double x_remainder = std::fmod((x_max_bound - x_min_bound), spacing_45);
    double y_remainder = std::fmod((y_max_bound - y_min_bound), spacing_45);

    x = x_min_bound;
    y = y_min_bound;

    gcode << move_to(Vec2d(x, y), writer, "Move to fill start");

    for (int i = 0; i < x_count + y_count +
                            (x_remainder + y_remainder >= spacing_45 ? 1 : 0);
         ++i) { // this isn't the most robust way, but less expensive than
                // finding line intersections
        if (i < std::min(x_count, y_count)) {
            if (i % 2 == 0) {
                x += spacing_45;
                y = y_min_bound;
                gcode << move_to(Vec2d(x, y), writer, "Fill: Step right");

                y += x - x_min_bound;
                x       = x_min_bound;
                comment = "Fill: Print up/left";
                gcode << draw_line(writer, Vec2d(x, y), line_arg_line_width,
                                   line_arg_height, line_arg_speed, comment);
            } else {
                y += spacing_45;
                x = x_min_bound;
                gcode << move_to(Vec2d(x, y), writer, "Fill: Step up");

                x += y - y_min_bound;
                y       = y_min_bound;
                comment = "Fill: Print down/right";
                gcode << draw_line(writer, Vec2d(x, y), line_arg_line_width,
                                   line_arg_height, line_arg_speed, comment);
            }
        } else if (i < std::max(x_count, y_count)) {
            if (x_count > y_count) {
                // box is wider than tall
                if (i % 2 == 0) {
                    x += spacing_45;
                    y = y_min_bound;
                    gcode << move_to(Vec2d(x, y), writer, "Fill: Step right");

                    x -= y_max_bound - y_min_bound;
                    y       = y_max_bound;
                    comment = "Fill: Print up/left";
                    gcode << draw_line(writer, Vec2d(x, y),
                                       line_arg_line_width, line_arg_height,
                                       line_arg_speed, comment);
                } else {
                    if (i == y_count) {
                        x += spacing_45 - y_remainder;
                        y_remainder = 0;
                    } else {
                        x += spacing_45;
                    }
                    y = y_max_bound;
                    gcode << move_to(Vec2d(x, y), writer, "Fill: Step right");

                    x += y_max_bound - y_min_bound;
                    y       = y_min_bound;
                    comment = "Fill: Print down/right";
                    gcode << draw_line(writer, Vec2d(x, y),
                                       line_arg_line_width, line_arg_height,
                                       line_arg_speed, comment);
                }
            } else {
                // box is taller than wide
                if (i % 2 == 0) {
                    x = x_max_bound;
                    if (i == x_count) {
                        y += spacing_45 - x_remainder;
                        x_remainder = 0;
                    } else {
                        y += spacing_45;
                    }
                    gcode << move_to(Vec2d(x, y), writer, "Fill: Step up");

                    x = x_min_bound;
                    y += x_max_bound - x_min_bound;
                    comment = "Fill: Print up/left";
                    gcode << draw_line(writer, Vec2d(x, y),
                                       line_arg_line_width, line_arg_height,
                                       line_arg_speed, comment);
                } else {
                    x = x_min_bound;
                    y += spacing_45;
                    gcode << move_to(Vec2d(x, y), writer, "Fill: Step up");

                    x = x_max_bound;
                    y -= x_max_bound - x_min_bound;
                    comment = "Fill: Print down/right";
                    gcode << draw_line(writer, Vec2d(x, y),
                                       line_arg_line_width, line_arg_height,
                                       line_arg_speed, comment);
                }
            }
        } else {
            if (i % 2 == 0) {
                x = x_max_bound;
                if (i == x_count) {
                    y += spacing_45 - x_remainder;
                } else {
                    y += spacing_45;
                }
                gcode << move_to(Vec2d(x, y), writer, "Fill: Step up");

                x -= y_max_bound - y;
                y       = y_max_bound;
                comment = "Fill: Print up/left";
                gcode << draw_line(writer, Vec2d(x, y), line_arg_line_width,
                                   line_arg_height, line_arg_speed, comment);
            } else {
                if (i == y_count) {
                    x += spacing_45 - y_remainder;
                } else {
                    x += spacing_45;
                }
                y = y_max_bound;
                gcode << move_to(Vec2d(x, y), writer, "Fill: Step right");

                y -= x_max_bound - x;
                x       = x_max_bound;
                comment = "Fill: Print down/right";
                gcode << draw_line(writer, Vec2d(x, y), line_arg_line_width,
                                   line_arg_height, line_arg_speed, comment);
            }
        }
    }

    return gcode.str();
}

double CalibPressureAdvance::get_distance(Vec2d from, Vec2d to) const
{
    return std::hypot((to.x() - from.x()), (to.y() - from.y()));
}

std::string CalibPressureAdvance::draw_line(GCodeWriter       &writer,
                                            Vec2d              to_pt,
                                            double             line_width,
                                            double             layer_height,
                                            double             speed,
                                            const std::string &comment)
{
    const double e_per_mm = CalibPressureAdvance::e_per_mm(
        line_width, layer_height,
        m_config.option<ConfigOptionFloats>("nozzle_diameter")->get_at(0),
        m_config.option<ConfigOptionFloats>("filament_diameter")->get_at(0),
        m_config.option<ConfigOptionFloats>("extrusion_multiplier")->get_at(0));

    const double length = get_distance(Vec2d(m_last_pos.x(), m_last_pos.y()),
                                       to_pt);
    auto         dE     = e_per_mm * length;

    std::stringstream gcode;

    gcode << writer.set_speed(speed);
    gcode << writer.extrude_to_xy(to_pt, dE, comment);

    m_last_pos = Vec3d(to_pt.x(), to_pt.y(), 0);

    return gcode.str();
}

CalibPressureAdvanceLine::CalibPressureAdvanceLine(GCodeGenerator *gcodegen)
    : CalibPressureAdvance(gcodegen->config())
    , mp_gcodegen(gcodegen)
    , m_nozzle_diameter(gcodegen->config().nozzle_diameter.get_at(0))
{}

std::string CalibPressureAdvanceLine::generate_test(double start_pa /*= 0*/,
                                                    double step_pa /*= 0.002*/,
                                                    int count /*= 10*/)
{
    BoundingBoxf bed_ext = get_extents(mp_gcodegen->config().bed_shape.values);

    const auto &w = bed_ext.size().x();
    const auto &h = bed_ext.size().y();
    count         = std::min(count, int((h - 10) / m_space_y));

    m_length_long = 40 + std::min(w - 120.0, 0.0);

    auto startx = (w - m_length_short * 2 - m_length_long - 20) / 2;
    auto starty = (h - count * m_space_y) / 2;

    return print_pa_lines(startx, starty, start_pa, step_pa, count);
}

std::string CalibPressureAdvanceLine::print_pa_lines(
    double start_x, double start_y, double start_pa, double step_pa, int num)
{
    auto       &writer = mp_gcodegen->writer();
    const auto &config = mp_gcodegen->config();

    const auto filament_diameter = config.filament_diameter.get_at(0);
    const auto print_flow_ratio  = config.extrusion_multiplier.get_at(0);

    const double e_per_mm = CalibPressureAdvance::e_per_mm(m_line_width,
                                                           m_height_layer,
                                                           m_nozzle_diameter,
                                                           filament_diameter,
                                                           print_flow_ratio);
    const double thin_e_per_mm =
        CalibPressureAdvance::e_per_mm(m_thin_line_width, m_height_layer,
                                       m_nozzle_diameter, filament_diameter,
                                       print_flow_ratio);
    const double number_e_per_mm =
        CalibPressureAdvance::e_per_mm(m_number_line_width, m_height_layer,
                                       m_nozzle_diameter, filament_diameter,
                                       print_flow_ratio);

    const double      fast = CalibPressureAdvance::speed_adjust(m_fast_speed);
    const double      slow = CalibPressureAdvance::speed_adjust(m_slow_speed);
    std::stringstream gcode;
    gcode << mp_gcodegen->writer().travel_to_z(m_height_layer);
    double y_pos = start_y;

    // prime line
    auto prime_x = start_x;
    gcode << move_to(Vec2d(prime_x, y_pos + (num) *m_space_y), writer);
    gcode << writer.set_speed(slow);
    gcode << writer.extrude_to_xy(Vec2d(prime_x, y_pos),
                                  e_per_mm * m_space_y * num * 1.2);

    for (int i = 0; i < num; ++i) {
        gcode << writer.set_pressure_advance(start_pa + i * step_pa);
        gcode << move_to(Vec2d(start_x, y_pos + i * m_space_y), writer);
        gcode << writer.set_speed(slow);
        gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short,
                                            y_pos + i * m_space_y),
                                      e_per_mm * m_length_short);
        gcode << writer.set_speed(fast);
        gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short +
                                                m_length_long,
                                            y_pos + i * m_space_y),
                                      e_per_mm * m_length_long);
        gcode << writer.set_speed(slow);
        gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short +
                                                m_length_long + m_length_short,
                                            y_pos + i * m_space_y),
                                      e_per_mm * m_length_short);
    }
    gcode << writer.set_pressure_advance(0.0);

    if (m_draw_numbers) {
        gcode << writer.set_speed(fast);
        gcode << move_to(Vec2d(start_x + m_length_short,
                               y_pos + (num - 1) * m_space_y + 2),
                         writer);
        gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short,
                                            y_pos + (num - 1) * m_space_y + 7),
                                      thin_e_per_mm * 7);
        gcode << move_to(Vec2d(start_x + m_length_short + m_length_long,
                               y_pos + (num - 1) * m_space_y + 7),
                         writer);
        gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short +
                                                m_length_long,
                                            y_pos + (num - 1) * m_space_y + 2),
                                      thin_e_per_mm * 7);

        const auto box_start_x = start_x + m_length_short + m_length_long +
                                 m_length_short;
        DrawBoxOptArgs default_box_opt_args(2, m_height_layer, m_line_width,
                                            fast);
        default_box_opt_args.is_filled = true;
        gcode << draw_box(writer, box_start_x, start_y - m_space_y,
                          number_spacing() * 8, (num + 1) * m_space_y,
                          default_box_opt_args);
        gcode << writer.travel_to_z(m_height_layer * 2);
        for (int i = 0; i < num; i += 2) {
            gcode << draw_number(box_start_x + 3 + m_line_width,
                                 y_pos + i * m_space_y + m_space_y / 2,
                                 start_pa + i * step_pa, m_draw_digit_mode,
                                 m_number_line_width, number_e_per_mm, 3600,
                                 writer);
        }
    }
    return gcode.str();
}

void CalibPressureAdvanceLine::delta_modify_start(double &startx,
                                                  double &starty,
                                                  int     count)
{
    startx = -startx;
    starty = -(count * m_space_y) / 2;
}
} // namespace Slic3r
