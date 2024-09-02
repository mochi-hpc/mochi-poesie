/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_STRING_VIEW_H
#define __POESIE_STRING_VIEW_H

#include <cereal/cereal.hpp>

namespace cereal
{
    template <class Archive, class CharT, class Traits> inline
    void save( Archive & ar, std::basic_string_view<CharT, Traits> const & str )
    {
        ar(make_size_tag(static_cast<size_type>(str.size())));
        ar(binary_data(str.data(), str.size() * sizeof(CharT)));
    }
}

#endif
