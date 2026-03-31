#pragma once

template<typename Set>
auto closest_element(Set& set, const typename Set::value_type& value) -> decltype(set.begin())
{
    const auto it = set.lower_bound(value);
    if (it == set.begin())
        return it;

    const auto prev_it = std::prev(it);
    return (it == set.end() || value - *prev_it <= *it - value) ? prev_it : it;
}
