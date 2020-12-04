/*
    This file is part of Waver

    Copyright (C) 2017-2019 Peter Papp <peter.papp.p@gmail.com>

    Please visit https://launchpad.net/waver for details

    Waver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Waver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    (GPL.TXT) along with Waver. If not, see <http://www.gnu.org/licenses/>.

*/


package rocks.waver.waver;

import android.content.Intent;
import android.os.Bundle;
import org.qtproject.qt5.android.bindings.QtActivity;
import rocks.waver.waver.WaverService;


public class WaverActivity extends QtActivity {

    @Override
    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);

        Intent serviceIntent = new Intent(this, rocks.waver.waver.WaverService.class);
        startService(serviceIntent);
    }


    public void stopService() {
        Intent serviceIntent = new Intent(this, rocks.waver.waver.WaverService.class);
        stopService(serviceIntent);
    }

}
