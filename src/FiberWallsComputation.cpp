//Copyright (c) 2018 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#include "FiberWallsComputation.h"
#include "utils/polygonUtils.h"
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
        EWallsToReinforce walls_to_reinforce = settings.get<EWallsToReinforce>("reinforcement_walls_to_reinforce");
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
                offsetted = part->fiber_insets[i - 1].offset(-line_width, ClipperLib::jtSquare);
            }
            if (walls_to_reinforce == EWallsToReinforce::ALL)
            {
                part->fiber_insets[i] = offsetted;
            }
            else
            {
                for (size_t j = 0; j < offsetted.size(); j++)
                {
                    const bool is_outer = offsetted[j].orientation();
                    if (walls_to_reinforce == EWallsToReinforce::INNER && !is_outer)
                    {
                        part->fiber_insets.back().add(offsetted[j]);
                    }
                    else if (walls_to_reinforce == EWallsToReinforce::OUTER && is_outer)
                    {
                        part->fiber_insets.back().add(offsetted[j]);
                    }
                }
            }

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
