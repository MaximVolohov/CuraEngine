//Copyright (c) 2018 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#include "FiberWallsComputation.h"
#include "utils/polygonUtils.h"
#include "utils/linearAlg2D.h"
namespace cura
{

    FiberWallsComputation::FiberWallsComputation(const Settings &settings, const LayerIndex layer_nr)
        : settings(settings), layer_nr(layer_nr)
    {
    }

    /*
 * This function is executed in a parallel region based on layer_nr.
 * When modifying make sure any changes does not introduce data races.
 *
 * generateInsets only reads and writes data for the current layer
 */
    void FiberWallsComputation::generateInsets(SliceLayerPart *part)
    {
        const bool reinforcement_enabled = settings.get<bool>("reinforcement_enabled");
        const size_t reinforcement_start_layer = settings.get<size_t>("reinforcement_start_layer");
        const size_t reinforcement_layer_count = settings.get<size_t>("reinforcement_layer_count");
        size_t fiber_inset_count = settings.get<size_t>("reinforcement_concentric_fiber_rings");

        if (!reinforcement_enabled || layer_nr < reinforcement_start_layer || layer_nr >= (reinforcement_start_layer + reinforcement_layer_count) || fiber_inset_count == 0)
        {
            return;
        }
        coord_t line_width = settings.get<coord_t>("fiber_infill_line_width");

        size_t inset_count = settings.get<size_t>("wall_line_count");
        coord_t line_width_0 = settings.get<coord_t>("wall_line_width_0");
        coord_t line_width_x = settings.get<coord_t>("wall_line_width_x");
        coord_t inset_offset = inset_count > 1 ? line_width_x / 2 : line_width_0 / 2;
        for (size_t i = 0; i < fiber_inset_count; i++)
        {
            part->fiber_insets.push_back(Polygons());
            Polygons offsetted;
            if (i == 0)
            {
                offsetted = part->insets.back().offset(-inset_offset - line_width / 2, ClipperLib::jtSquare);
            }
            else
            {
                offsetted = part->insets.back().offset(-inset_offset - line_width / 2 - line_width * i, ClipperLib::jtSquare);
            }
            part->fiber_insets[i] = offsetted;

            //const size_t inset_part_count = part->fiber_insets[i].size();
            //constexpr size_t minimum_part_saving = 3; //Only try if the part has more pieces than the previous inset and saves at least this many parts.
            //constexpr coord_t try_smaller = 10;       //How many micrometres to inset with the try with a smaller inset.
            //if (inset_part_count > minimum_part_saving + 1 && (i == 0 || (i > 0 && inset_part_count > part->insets[i - 1].size() + minimum_part_saving)))
            //{
            //    //Try a different line thickness and see if this fits better, based on these criteria:
            //    // - There are fewer parts to the polygon (fits better in slim areas).
            //    // - The polygon area is largely unaffected.
            //    Polygons alternative_inset;
            //    if (i == 0)
            //    {
            //        alternative_inset = part->outline.offset(-(line_width_0 - try_smaller) / 2 - wall_0_inset);
            //    }
            //    else if (i == 1)
            //    {
            //        alternative_inset = part->insets[0].offset(-(line_width_0 - try_smaller) / 2 + wall_0_inset - line_width_x / 2);
            //    }
            //    else
            //    {
            //        alternative_inset = part->insets[i - 1].offset(-(line_width_x - try_smaller));
            //    }
            //    if (alternative_inset.size() < inset_part_count - minimum_part_saving) //Significantly fewer parts (saves more than 3 parts).
            //    {
            //        part->insets[i] = alternative_inset;
            //    }
            //}

            //Finally optimize all the polygons. Every point removed saves time in the long run.
            part->fiber_insets[i].simplify();
            part->fiber_insets[i].removeDegenerateVerts();
            //if (i == 0)
            //{
            //    if (recompute_outline_based_on_outer_wall)
            //    {
            //        part->print_outline = part->insets[0].offset(line_width_0 / 2, ClipperLib::jtSquare);
            //    }
            //    else
            //    {
            //        part->print_outline = part->outline;
            //    }
            //}
            if (part->fiber_insets[i].size() < 1)
            {
                part->fiber_insets.pop_back();
                break;
            }
            if (true) //(i == 0)
            {
                coord_t min_fiber_line_length = settings.get<coord_t>("reinforcement_min_fiber_line_length");
                for (size_t inset_idx = 0; inset_idx < part->fiber_insets[i].size(); inset_idx++)
                {
                    PolygonRef poly = part->fiber_insets[i][inset_idx];
                    bool obtuse_angle = LinearAlg2D::isAcuteCorner(poly.back(), poly[0], poly[1]) < 0;
                    if (!obtuse_angle)
                    {
                        //try to find obtuse angle
                        int start_idx = 0;
                        for (size_t point_idx = 1; point_idx < poly.size(); ++point_idx)
                        {
                            if (LinearAlg2D::isAcuteCorner(poly[(point_idx + 1) % poly.size()], poly[point_idx], poly[point_idx - 1]) < 0)
                            {
                                start_idx = point_idx;
                                break;
                            }
                        }
                        //we found obtuse start
                        if (start_idx > 0)
                        {
                            part->fiber_insets[i][inset_idx][0] = poly[start_idx];
                            for (unsigned int point_idx = 1; point_idx < poly.size(); point_idx++)
                            {
                                part->fiber_insets[i][inset_idx][point_idx] = poly[(point_idx + start_idx) % poly.size()];
                            }
                        }
                        else
                        {
                            //obtuse start not found
                            Point p0 = poly[0];
                            for (size_t point_idx = 1; point_idx < poly.size(); point_idx++)
                            {
                                Point p1 = poly[point_idx];
                                if (vSize2(p1 - p0) > (min_fiber_line_length * min_fiber_line_length))
                                {
                                    double min_dist = std::max(vSizeMM(p1 - p0) / 2.0, INT2MM(min_fiber_line_length));
                                    double ratio = min_dist / (vSizeMM(p1 - p0));
                                    Point px = (p1 - p0) * ratio + p0;
                                    poly.add(poly[0]);
                                    poly[0] = px;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /*
 * This function is executed in a parallel region based on layer_nr.
 * When modifying make sure any changes does not introduce data races.
 *
 * generateInsets only reads and writes data for the current layer
 */
    void FiberWallsComputation::generateInsets(SliceLayer *layer)
    {
        for (unsigned int partNr = 0; partNr < layer->parts.size(); partNr++)
        {
            generateInsets(&layer->parts[partNr]);
        }

        //const bool remove_parts_with_no_insets = !settings.get<bool>("fill_outline_gaps");
        ////Remove the parts which did not generate an inset. As these parts are too small to print,
        //// and later code can now assume that there is always minimal 1 inset line.
        //for (unsigned int part_idx = 0; part_idx < layer->parts.size(); part_idx++)
        //{
        //    if (layer->parts[part_idx].insets.size() == 0 && remove_parts_with_no_insets)
        //    {
        //        if (part_idx != layer->parts.size() - 1)
        //        { // move existing part into part to be deleted
        //            layer->parts[part_idx] = std::move(layer->parts.back());
        //        }
        //        layer->parts.pop_back(); // always remove last element from array (is more efficient)
        //        part_idx -= 1;           // check the part we just moved here
        //    }
        //}
    }

} //namespace cura
