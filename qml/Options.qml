import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3

Dialog {
    title: qsTr("Options")
    modal: true
    focus: true
    standardButtons: Dialog.Ok | Dialog.Apply | Dialog.Cancel

    property int imageSize: 36

    signal optionsSending(string optionsJSON)
    signal reqEQ(int eq_chooser)

    function setOptions(optionsObj)
    {
        if (optionsObj.eq_disable) {
            preAmp.enabled = false;
            eqOn.enabled = false;
            eq1.enabled = false;
            eq2.enabled = false;
            eq3.enabled = false;
            eq4.enabled = false;
            eq5.enabled = false;
            eq6.enabled = false;
            eq7.enabled = false;
            eq8.enabled = false;
            eq9.enabled = false;
            eq10.enabled = false;
            eqCommon.enabled = false;
            eqAlbum.enabled = false;
            eqSong.enabled = false;
        }
        else {
            preAmp.enabled = eqOn.checked;
            preAmp.value = optionsObj.pre_amp;

            eqOn.enabled = true;
            eqOn.checked = optionsObj.eq_on;

            eq1Label.text = optionsObj.eq1Label;
            eq1.enabled = eqOn.checked;
            eq1.value = optionsObj.eq1;
            eq2Label.text = optionsObj.eq2Label;
            eq2.enabled = eqOn.checked;
            eq2.value = optionsObj.eq2;
            eq3Label.text = optionsObj.eq3Label;
            eq3.enabled = eqOn.checked;
            eq3.value = optionsObj.eq3;
            eq4Label.text = optionsObj.eq4Label;
            eq4.enabled = eqOn.checked;
            eq4.value = optionsObj.eq4;
            eq5Label.text = optionsObj.eq5Label;
            eq5.enabled = eqOn.checked;
            eq5.value = optionsObj.eq5;
            eq6Label.text = optionsObj.eq6Label;
            eq6.enabled = eqOn.checked;
            eq6.value = optionsObj.eq6;
            eq7Label.text = optionsObj.eq7Label;
            eq7.enabled = eqOn.checked;
            eq7.value = optionsObj.eq7;
            eq8Label.text = optionsObj.eq8Label;
            eq8.enabled = eqOn.checked;
            eq8.value = optionsObj.eq8;
            eq9Label.text = optionsObj.eq9Label;
            eq9.enabled = eqOn.checked;
            eq9.value = optionsObj.eq9;
            eq10Label.text = optionsObj.eq10Label;
            eq10.enabled = eqOn.checked;
            eq10.value = optionsObj.eq10;
            eqCommon.enabled = eqOn.checked;
            eqCommon.checked = optionsObj.eq_chooser === 0;
            eqAlbum.enabled = eqOn.checked;
            eqAlbum.checked = optionsObj.eq_chooser === 1;
            eqSong.enabled = eqOn.checked;
            eqSong.checked = optionsObj.eq_chooser === 2;
        }
        shuffle_autostart.checked = optionsObj.shuffle_autostart;
        shuffle_delay_seconds.value = optionsObj.shuffle_delay_seconds;
        shuffle_count.value = optionsObj.shuffle_count;
        shuffle_favorite_frequency.currentIndex = optionsObj.shuffle_favorite_frequency <= internal.shuffle_favorite_frequent ? 2 : optionsObj.shuffle_favorite_frequency >= internal.shuffle_favorite_rare ? 0 : 1;
        shuffle_recently_added_frequency.currentIndex = optionsObj.shuffle_recently_added_frequency <= internal.shuffle_favorite_frequent ? 2 : optionsObj.shuffle_recently_added_frequency >= internal.shuffle_favorite_rare ? 0 : 1;
        shuffle_operator.currentIndex = optionsObj.shuffle_operator === "or" ? 1 : 0;
        random_lists_count.value = optionsObj.random_lists_count;
        recently_added_count.value = optionsObj.recently_added_count;
        recently_added_days.value = optionsObj.recently_added_days;

        search_count_max.value = optionsObj.search_count_max;
        search_action.currentIndex = optionsObj.search_action;
        search_action_filter.currentIndex = optionsObj.search_action_filter;
        search_action_count_max.value = optionsObj.search_action_count_max;

        hide_dot_playlists.checked = optionsObj.hide_dot_playlists;
        title_curly_special.checked = optionsObj.title_curly_special;
        starting_index_apply.checked = optionsObj.starting_index_apply;
        starting_index_days.value = optionsObj.starting_index_days;
        wideStereo.value = optionsObj.wide_stereo;
        skip_long_silence.checked = optionsObj.skip_long_silence
        skip_long_silence_seconds.value = optionsObj.skip_long_silence_seconds
        fade_tags.text = optionsObj.fade_tags;
        crossfade_tags.text = optionsObj.crossfade_tags;
        fade_seconds.value = optionsObj.fade_seconds;

        max_peak_fps.value = optionsObj.max_peak_fps;
        peak_delay_on.checked = optionsObj.peak_delay_on;
        peak_delay_ms.value = optionsObj.peak_delay_ms;
        alphabet_limit.value = optionsObj.alphabet_limit;
        font_size.value = optionsObj.font_size;

        shuffleItems.clear();
        for (var i = 0; i < optionsObj.genres.length; i++) {
            var newGenre = {
                selected: optionsObj.genres[i].selected,
                title: optionsObj.genres[i].title
            }
            shuffleItems.append(newGenre);
        }
    }

    function setEQ(eqObj)
    {
        preAmp.value = eqObj.pre_amp;

        eq1.value = eqObj.eq1;
        eq2.value = eqObj.eq2;
        eq3.value = eqObj.eq3;
        eq4.value = eqObj.eq4;
        eq5.value = eqObj.eq5;
        eq6.value = eqObj.eq6;
        eq7.value = eqObj.eq7;
        eq8.value = eqObj.eq8;
        eq9.value = eqObj.eq9;
        eq10.value = eqObj.eq10;
    }


    onAccepted: internal.sendOptions()
    onApplied: internal.sendOptions()

    QtObject {
        id: internal

        readonly property int shuffle_favorite_rare    : 15;
        readonly property int shuffle_favorite_normal  : 9;
        readonly property int shuffle_favorite_frequent: 4;

        function sendOptions()
        {
            var genres = [];
            for(var i = 0; i < shuffleItems.count; i++) {
                if (shuffleItems.get(i).selected) {
                    genres.push(shuffleItems.get(i).title);
                }
            }

            var optionsObj = {
                eq_disable: !eqOn.enabled && !eqOn.checked,
                eq_on: eqOn.checked,
                pre_amp: preAmp.value,
                eq1: eq1.value,
                eq2: eq2.value,
                eq3: eq3.value,
                eq4: eq4.value,
                eq5: eq5.value,
                eq6: eq6.value,
                eq7: eq7.value,
                eq8: eq8.value,
                eq9: eq9.value,
                eq10: eq10.value,
                eq_chooser: eqCommon.checked ? 0 : eqAlbum.checked ? 1 : 2,
                wide_stereo: wideStereo.value,
                skip_long_silence: skip_long_silence.checked,
                skip_long_silence_seconds: skip_long_silence_seconds.value,
                shuffle_autostart: shuffle_autostart.checked,
                shuffle_delay_seconds: shuffle_delay_seconds.value,
                shuffle_count: shuffle_count.value,
                random_lists_count: random_lists_count.value,
                recently_added_count: recently_added_count.value,
                recently_added_days: recently_added_days.value,
                shuffle_favorite_frequency: shuffle_favorite_frequency.currentIndex == 0 ? shuffle_favorite_rare : shuffle_favorite_frequency.currentIndex == 1 ? shuffle_favorite_normal : shuffle_favorite_frequent,
                shuffle_recently_added_frequency: shuffle_recently_added_frequency.currentIndex == 0 ? shuffle_favorite_rare : shuffle_recently_added_frequency.currentIndex == 1 ? shuffle_favorite_normal : shuffle_favorite_frequent,
                shuffle_operator: shuffle_operator.currentIndex == 0 ? 'and' : 'or',
                search_count_max: search_count_max.value,
                search_action: search_action.currentIndex,
                search_action_filter: search_action_filter.currentIndex,
                search_action_count_max: search_action_count_max.value,
                hide_dot_playlists: hide_dot_playlists.checked,
                title_curly_special: title_curly_special.checked,
                starting_index_apply: starting_index_apply.checked,
                starting_index_days: starting_index_days.value,
                fade_tags: fade_tags.text,
                crossfade_tags: crossfade_tags.text,
                fade_seconds: fade_seconds.value,
                max_peak_fps: max_peak_fps.value,
                peak_delay_on: peak_delay_on.checked,
                peak_delay_ms: peak_delay_ms.value,
                alphabet_limit: alphabet_limit.value,
                font_size: font_size.value,
                genres: genres
            };
            optionsSending(JSON.stringify(optionsObj));
        }
    }


    TabBar {
        id: optionsTabs

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        TabButton {
            text: qsTr("Equalizer")
            Switch {
                id: eqOn
                anchors.right: parent.right
                anchors.rightMargin: 7

                onCheckedChanged: {
                    preAmp.enabled = eqOn.checked;
                    eq1.enabled = eqOn.checked;
                    eq2.enabled = eqOn.checked;
                    eq3.enabled = eqOn.checked;
                    eq4.enabled = eqOn.checked;
                    eq5.enabled = eqOn.checked;
                    eq6.enabled = eqOn.checked;
                    eq7.enabled = eqOn.checked;
                    eq8.enabled = eqOn.checked;
                    eq9.enabled = eqOn.checked;
                    eq10.enabled = eqOn.checked;
                }
            }
        }
        TabButton {
            text: qsTr("Shuffle")
        }
        TabButton {
            text: qsTr("Search")
        }
        TabButton {
            text: qsTr("General")
        }
    }

    StackLayout {
        anchors.top: optionsTabs.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        currentIndex: optionsTabs.currentIndex

        Item {
            x: 10
            y: 10
            width: parent.width - 20
            height: parent.height - 20

            Row {
                id: eqChooser
                anchors.top: parent.top
                height: eqCommon.height

                RadioButton {
                    id: eqCommon
                    text: qsTr("Common")
                    onCheckedChanged: {
                        if (checked) {
                            reqEQ(0);
                        }
                    }
                }
                RadioButton {
                    id: eqAlbum
                    text: qsTr("Album")
                    onCheckedChanged: {
                        if (checked) {
                            reqEQ(1);
                        }
                    }
                }
                RadioButton {
                    id: eqSong
                    text: qsTr("Song")
                    onCheckedChanged: {
                        if (checked) {
                            reqEQ(2);
                        }
                    }
                }
            }
            Row {
                id: eqControls
                anchors.top: eqChooser.bottom
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right

                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: preAmp
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: preAmpLabel.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 6
                    }
                    Label {
                        id: preAmpLabel
                        text: qsTr("preAmp")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq1
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq1Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }
                    Label {
                        id: eq1Label
                        text: qsTr("EQ1")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq2
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq2Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }
                    Label {
                        id: eq2Label
                        text: qsTr("EQ2")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq3
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq3Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }
                    Label {
                        id: eq3Label
                        text: qsTr("EQ3")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq4
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq4Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }

                    Label {
                        id: eq4Label
                        text: qsTr("EQ4")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq5
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq5Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }
                    Label {
                        id: eq5Label
                        text: qsTr("EQ5")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq6
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq6Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }
                    Label {
                        id: eq6Label
                        text: qsTr("EQ6")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq7
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq7Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }
                    Label {
                        id: eq7Label
                        text: qsTr("EQ7")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq8
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq8Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }
                    Label {
                        id: eq8Label
                        text: qsTr("EQ8")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq9
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq9Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }
                    Label {
                        id: eq9Label
                        text: qsTr("EQ9")
                    }
                }
                Column {
                    height: parent.height
                    width: parent.width / 12
                    Slider {
                        id: eq10
                        height: parent.height - preAmpLabel.height
                        anchors.horizontalCenter: eq10Label.horizontalCenter
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                    }
                    Label {
                        id: eq10Label
                        text: qsTr("EQ10")
                    }
                }
            }
        }

        Flickable {
            x: 10
            y: 10
            width: parent.width - 20
            height: parent.height - 20
            clip: true
            contentHeight: shuffleColumn.height

            ScrollBar.vertical: ScrollBar { }

            Column {
                id: shuffleColumn
                width: parent.width - 20

                Row {
                    CheckBox {
                        id: shuffle_autostart
                        text: qsTr("Autostart Shuffle")
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: shuffle_delay_seconds.verticalCenter
                        text: qsTr("Delay Before Autostart")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    SpinBox {
                        id: shuffle_delay_seconds
                        from: 2
                        to: 60
                    }
                    Label {
                        anchors.rightMargin: 17
                        anchors.verticalCenter: shuffle_delay_seconds.verticalCenter
                        text: qsTr("seconds")
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: shuffle_count.verticalCenter
                        text: qsTr("Song Count")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    SpinBox {
                        id: shuffle_count
                        from: 3
                        to: 33
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: random_lists_count.verticalCenter
                        text: qsTr("Song Count of Random lists")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    SpinBox {
                        id: random_lists_count
                        from: 3
                        to: 33
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: shuffle_favorite_frequency.verticalCenter
                        text: qsTr("Favorites")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    ComboBox {
                        id: shuffle_favorite_frequency
                        width: parent.parent.width / 3 * 2
                        model: ListModel {
                            ListElement {
                              text: qsTr("Rare")
                            }
                            ListElement {
                              text: qsTr("Normal")
                            }
                            ListElement {
                              text: qsTr("Frequent")
                            }
                        }
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: shuffle_recently_added_frequency.verticalCenter
                        text: qsTr("Recently Added")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    ComboBox {
                        id: shuffle_recently_added_frequency
                        width: parent.parent.width / 3 * 2
                        model: ListModel {
                            ListElement {
                              text: qsTr("Rare")
                            }
                            ListElement {
                              text: qsTr("Normal")
                            }
                            ListElement {
                              text: qsTr("Frequent")
                            }
                        }
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: shuffle_genres.verticalCenter
                        text: qsTr("Genres")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    ListView {
                        id: shuffle_genres
                        width: parent.parent.width / 3 * 2
                        height: (imageSize + (imageSize / 12)) * shuffleItems.count

                        clip: true
                        highlight: Rectangle {
                            color: "LightSteelBlue";
                        }
                        highlightMoveDuration: 500
                        highlightMoveVelocity: 500
                        delegate: shuffleElement
                        model: shuffleItems

                        ScrollBar.vertical: ScrollBar {
                        }
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: shuffle_operator.verticalCenter
                        text: qsTr("Genres Operator")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    ComboBox {
                        id: shuffle_operator
                        width: parent.parent.width / 3 * 2
                        model: ListModel {
                            ListElement {
                              text: qsTr("AND - Match all")
                            }
                            ListElement {
                              text: qsTr("OR - Match any")
                            }
                        }
                    }
                }
                Row {
                    topPadding: 17
                    bottomPadding: 17

                    Rectangle {
                        x: 0
                        y: 0
                        width: parent.parent.width
                        height: 1
                        color: "#AAAAAA"
                    }
                }
                Row {
                    bottomPadding: 17
                    Label {
                        text: qsTr("<b>\"Recently Added\" Pool Size</b>")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                }

                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: recently_added_count.verticalCenter
                        text: qsTr("Count")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    SpinBox {
                        id: recently_added_count
                        from: 25
                        to: 500
                    }
                    Label {
                        anchors.rightMargin: 17
                        anchors.verticalCenter: recently_added_count.verticalCenter
                        text: qsTr("<i>or</i>")
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: recently_added_days.verticalCenter
                        text: qsTr("Days")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    SpinBox {
                        id: recently_added_days
                        from: 10
                        to: 90
                    }
                }
            }
        }

        Flickable {
            x: 10
            y: 10
            width: parent.width - 20
            height: parent.height - 20
            clip: true
            contentHeight: searchColumn.height

            ScrollBar.vertical: ScrollBar { }

            Column {
                id: searchColumn
                width: parent.width - 20

                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: search_count_max.verticalCenter
                        text: qsTr("Maximum Count")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    SpinBox {
                        id: search_count_max
                        from: 0
                        to: 99
                    }
                    Label {
                        anchors.rightMargin: 17
                        anchors.verticalCenter: search_count_max.verticalCenter
                        text: qsTr("<i>(0 means unlmimited)</i>")
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: search_action.verticalCenter
                        text: qsTr("Action")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    ComboBox {
                        id: search_action
                        width: parent.parent.width / 3 * 2
                        model: ListModel {
                            ListElement {
                                text: qsTr("None")
                            }
                            ListElement {
                                text: qsTr("Play")
                            }
                            ListElement {
                                text: qsTr("Play Next")
                            }
                            ListElement {
                                text: qsTr("Enqueue")
                            }
                            ListElement {
                                text: qsTr("Randomize")
                            }
                        }
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: search_action_filter.verticalCenter
                        text: qsTr("Filter for action")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    ComboBox {
                        id: search_action_filter
                        width: parent.parent.width / 3 * 2
                        model: ListModel {
                            ListElement {
                                text: qsTr("None")
                            }
                            ListElement {
                                text: qsTr("Starts With")
                            }
                            ListElement {
                                text: qsTr("Exact Match")
                            }
                            ListElement {
                                text: qsTr("Exact Match OR Starts With OR None")
                            }
                        }
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 3
                        anchors.verticalCenter: search_action_count_max.verticalCenter
                        text: qsTr("Maximum Count For Action")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    SpinBox {
                        id: search_action_count_max
                        from: 0
                        to: 99
                    }
                    Label {
                        anchors.rightMargin: 17
                        anchors.verticalCenter: search_action_count_max.verticalCenter
                        text: qsTr("<i>(0 means unlmimited)</i>")
                    }
                }
            }
        }

        Flickable {
            x: 10
            y: 10
            width: parent.width - 20
            height: parent.height - 20
            clip: true
            contentHeight: generalColumn.height

            ScrollBar.vertical: ScrollBar { }

            Column {
                id: generalColumn
                width: parent.width - 20

                Row {
                    CheckBox {
                        id: hide_dot_playlists
                        text: qsTr("Hide playlists whose name starts with a dot")
                    }
                }
                Row {
                    CheckBox {
                        id: title_curly_special
                        text: qsTr("\"{\" in title indicates extra info")
                    }
                }
                Row {
                    CheckBox {
                        id: starting_index_apply
                        anchors.verticalCenter: starting_index_days.verticalCenter
                        text: qsTr("Remember playlists' last played track for")
                    }
                    SpinBox {
                        id: starting_index_days
                        editable: true
                        from: 3
                        to: 365
                    }
                    Label {
                        anchors.rightMargin: 17
                        anchors.verticalCenter: starting_index_days.verticalCenter
                        text: qsTr("days")
                    }
                }
                Row {
                    topPadding: 17
                    bottomPadding: 17

                    Rectangle {
                        x: 0
                        y: 0
                        width: parent.parent.width
                        height: 1
                        color: "#AAAAAA"
                    }
                }
                Row {
                    bottomPadding: 17
                    Label {
                        text: qsTr("<b>Appearance</b>")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 4
                        anchors.verticalCenter: font_size.verticalCenter
                        text: qsTr("Font Size")
                    }
                    SpinBox {
                        id: font_size
                        editable: true
                        from: 8
                        to: 16
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 4
                        anchors.verticalCenter: alphabet_limit.verticalCenter
                        text: qsTr("Group Items Alphabetically")
                    }
                    SpinBox {
                        id: alphabet_limit
                        editable: true
                        from: 9
                        to: 99
                    }
                }
                Row {
                    topPadding: 17
                    bottomPadding: 17

                    Rectangle {
                        x: 0
                        y: 0
                        width: parent.parent.width
                        height: 1
                        color: "#AAAAAA"
                    }
                }
                Row {
                    bottomPadding: 17
                    Label {
                        text: qsTr("<b>Sound</b>")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 4
                        anchors.rightMargin: 17
                        anchors.verticalCenter: wideStereo.verticalCenter
                        text: qsTr("Wide Stereo")
                    }
                    SpinBox {
                        id: wideStereo
                        editable: true
                        from: 0
                        to: 40
                    }
                    Label {
                        anchors.rightMargin: 17
                        anchors.verticalCenter: wideStereo.verticalCenter
                        text: qsTr("milliseconds")
                    }
                }
                Row {
                    CheckBox {
                        id: skip_long_silence
                        width: parent.parent.width / 4
                        anchors.verticalCenter: skip_long_silence_seconds.verticalCenter
                        text: qsTr("Skip silence longer than")
                    }
                    SpinBox {
                        id: skip_long_silence_seconds
                        editable: true
                        from: 3
                        to: 7
                    }
                    Label {
                        anchors.rightMargin: 17
                        anchors.verticalCenter: skip_long_silence_seconds.verticalCenter
                        text: qsTr("seconds")
                    }
                }
                Row {
                    topPadding: 17
                    bottomPadding: 17

                    Rectangle {
                        x: 0
                        y: 0
                        width: parent.parent.width
                        height: 1
                        color: "#AAAAAA"
                    }
                }
                Row {
                    bottomPadding: 17
                    Label {
                        text: qsTr("<b>Peak Meter</b>")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                }
                Row {
                    CheckBox {
                        width: parent.parent.width / 4
                        anchors.verticalCenter: peak_delay_ms.verticalCenter
                        id: peak_delay_on
                        text: qsTr("Delay")
                    }
                    SpinBox {
                        id: peak_delay_ms
                        editable: true
                        from: 5
                        to: 3000
                    }
                    Label {
                        anchors.rightMargin: 17
                        anchors.verticalCenter: peak_delay_ms.verticalCenter
                        text: qsTr("milliseconds <i>(useful for Bluetooth headphones/speakers)</i>")
                    }
                }
                Row {
                    leftPadding: 9
                    Label {
                        width: parent.parent.width / 4 - 9
                        anchors.verticalCenter: max_peak_fps.verticalCenter
                        text: qsTr("Maximum Frames Per Second")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    SpinBox {
                        id: max_peak_fps
                        from: 10
                        to: 50
                    }
                }
                Row {
                    topPadding: 17
                    bottomPadding: 17

                    Rectangle {
                        x: 0
                        y: 0
                        width: parent.parent.width
                        height: 1
                        color: "#AAAAAA"
                    }
                }
                Row {
                    bottomPadding: 17
                    Label {
                        text: qsTr("<b>Fade In / Fade Out</b>")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 4
                        anchors.verticalCenter: fade_tags.verticalCenter
                        text: qsTr("Fade Tags")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    TextField {
                        id: fade_tags
                        width: parent.parent.width / 4 * 3
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 4
                        anchors.verticalCenter: crossfade_tags.verticalCenter
                        text: qsTr("Crossfade Tags")
                        wrapMode: Label.WrapAtWordBoundaryOrAnywhere
                    }
                    TextField {
                        id: crossfade_tags
                        width: parent.parent.width / 4 * 3
                    }
                }
                Row {
                    Label {
                        width: parent.parent.width / 4
                        anchors.verticalCenter: fade_seconds.verticalCenter
                        text: qsTr("Fade Length")
                    }
                    SpinBox {
                        id: fade_seconds
                        editable: true
                        from: 3
                        to: 20
                    }
                    Label {
                        anchors.rightMargin: 17
                        anchors.verticalCenter: fade_seconds.verticalCenter
                        text: qsTr("seconds")
                    }
                }
            }
        }
    }

    Component {
        id: shuffleElement

        MouseArea {
            id: shuffleElementMouseArea

            anchors.left: parent ? parent.left : shuffleElement.left;
            anchors.right: parent ? parent.right : shuffleElement.right;
            height: shuffleElementItem.height

            acceptedButtons: Qt.LeftButton

            onClicked: {
                if (mouse.button == Qt.LeftButton) {
                    shuffleItems.get(index).selected = !shuffleItems.get(index).selected;
                }
            }

            Item {
                id: shuffleElementItem

                height: imageSize + (imageSize / 12)
                width: parent.width

                Image {
                    id: selectedImage

                    anchors.left: parent.left
                    anchors.leftMargin: 5
                    anchors.verticalCenter: parent.verticalCenter

                    height: imageSize - 4 < 18 ? imageSize - 4 : 18
                    width: imageSize - 4 < 18 ? imageSize - 4 : 18

                    source: selected ? "qrc:///icons/check_checked.ico" : "qrc:///icons/check_unchecked.ico"
                }
                Label {
                    id: genreLabel

                    anchors.left: selectedImage.right
                    anchors.leftMargin: 5
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter

                    elide: "ElideMiddle"
                    text: title
                }
            }
        }
    }

    ListModel {
        id: shuffleItems
    }
}
