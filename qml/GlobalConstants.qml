import QtQuick 2.0

Item {
    readonly property int action_collapse       : 1
    readonly property int action_enqueue        : 2
    readonly property int action_enqueueshuffled: 3
    readonly property int action_expand         : 4
    readonly property int action_insert         : 5
    readonly property int action_noop           : 6
    readonly property int action_move_to_top    : 7
    readonly property int action_play           : 8
    readonly property int action_playnext       : 9
    readonly property int action_refresh        : 10
    readonly property int action_remove         : 11
    readonly property int action_select         : 12
    readonly property int action_select_group   : 13
    readonly property int action_select_all     : 14
    readonly property int action_deselect_all   : 15

    readonly property int search_action_none     : 0
    readonly property int search_action_play     : 1
    readonly property int search_action_playnext : 2
    readonly property int search_action_enqueue  : 3
    readonly property int search_action_randomize: 4

    readonly property int search_action_filter_none      : 0
    readonly property int search_action_filter_startswith: 1
    readonly property int search_action_filter_exactmatch: 2
    readonly property int search_action_filter_reductive : 3
}
