#pragma once

#include "GCode/GCodeWriter.hpp"
#include "PrintConfig.hpp"
#include "BoundingBox.hpp"

namespace Slic3r {

class GCodeGenerator;

enum class CalibMode : int { Calib_None = 0, Calib_PA_Line };

struct Calib_Params
{
    Calib_Params() : mode(CalibMode::Calib_None) {}
    double    start, end, step;
    bool      print_numbers = false;
    CalibMode mode;
};

struct DrawBoxOptArgs
{
    DrawBoxOptArgs(int    num_perimeters,
                   double height,
                   double line_width,
                   double speed)
        : num_perimeters{num_perimeters}
        , height{height}
        , line_width{line_width}
        , speed{speed} {};
    DrawBoxOptArgs() = default;

    bool   is_filled{false};
    int    num_perimeters;
    double height;
    double line_width;
    double speed;
};

class CalibPressureAdvance
{
protected:
    CalibPressureAdvance() = default;
    CalibPressureAdvance(const DynamicPrintConfig &config)
        : m_config(config){};
    CalibPressureAdvance(const FullPrintConfig &config)
    {
        m_config.apply(config);
    };
    ~CalibPressureAdvance() = default;

    enum class DrawDigitMode { Left_To_Right, Bottom_To_Top };

    void delta_scale_bed_ext(BoundingBoxf &bed_ext) const
    {
        bed_ext.scale(1.0f / 1.41421f);
    }

    std::string move_to(Vec2d        pt,
                        GCodeWriter &writer,
                        std::string  comment = std::string());
    double      e_per_mm(double line_width,
                         double layer_height,
                         float  nozzle_diameter,
                         float  filament_diameter,
                         float  print_flow_ratio) const;
    double      speed_adjust(int speed) const { return speed * 60; };

    std::string convert_number_to_string(double num) const;
    double      number_spacing() const
    {
        return m_digit_segment_len + m_digit_gap_len;
    };
    std::string draw_digit(double                              startx,
                           double                              starty,
                           char                                c,
                           CalibPressureAdvance::DrawDigitMode mode,
                           double                              line_width,
                           double                              e_per_mm,
                           GCodeWriter                        &writer);
    std::string draw_number(double                              startx,
                            double                              starty,
                            double                              value,
                            CalibPressureAdvance::DrawDigitMode mode,
                            double                              line_width,
                            double                              e_per_mm,
                            double                              speed,
                            GCodeWriter                        &writer);
    std::string draw_box(GCodeWriter   &writer,
                         double         min_x,
                         double         min_y,
                         double         size_x,
                         double         size_y,
                         DrawBoxOptArgs opt_args);
    std::string draw_line(GCodeWriter       &writer,
                          Vec2d              to_pt,
                          double             line_width,
                          double             layer_height,
                          double             speed,
                          const std::string &comment = std::string());

    double to_radians(double degrees) const { return degrees * M_PI / 180; };
    double get_distance(Vec2d from, Vec2d to) const;

    Vec3d              m_last_pos;
    DynamicPrintConfig m_config;

    const double  m_encroachment{1. / 3.};
    DrawDigitMode m_draw_digit_mode{DrawDigitMode::Left_To_Right};
    const double  m_digit_segment_len{2};
    const double  m_digit_gap_len{1};
    const std::string::size_type m_max_number_len{5};
};

class CalibPressureAdvanceLine : public CalibPressureAdvance
{
public:
    CalibPressureAdvanceLine(GCodeGenerator *gcodegen);
    ~CalibPressureAdvanceLine(){};

    std::string generate_test(double start_pa = 0,
                              double step_pa  = 0.002,
                              int    count    = 50);

    void set_speed(double fast = 100.0, double slow = 20.0)
    {
        m_slow_speed = slow;
        m_fast_speed = fast;
    }

    const double &line_width() { return m_line_width; };
    // bool          is_delta() const;
    bool &draw_numbers() { return m_draw_numbers; }

private:
    std::string print_pa_lines(double start_x,
                               double start_y,
                               double start_pa,
                               double step_pa,
                               int    num);

    void delta_modify_start(double &startx, double &starty, int count);

    GCodeGenerator *mp_gcodegen;

    double m_nozzle_diameter;
    double m_slow_speed, m_fast_speed;

    const double m_height_layer{0.2};
    const double m_line_width{0.6};
    const double m_thin_line_width{0.44};
    const double m_number_line_width{0.48};
    const double m_space_y{3.5};

    double m_length_short{20.0}, m_length_long{40.0};
    bool   m_draw_numbers{true};
};

} // namespace Slic3r
