package com.example.rerun.app;

import java.util.Arrays;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Toast;
import eu.chainfire.libsuperuser.Shell;


public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        // http://themakeinfo.com/2015/05/building-android-root-app/
        if (Shell.SU.available()) {
            Shell.SU.run(Arrays.asList(
                    "am force-stop jp.co.bandainamcoent.BNEI0242",
                    "rm /data/data/jp.co.bandainamcoent.BNEI0242/shared_prefs/jp.co.bandainamcoent.BNEI0242.xml",
                    "cd /sdcard",
                    "nohup res_mara &"
            ));
        } else {
            Toast.makeText(this, "SU is not available", Toast.LENGTH_LONG).show();
        }
        finish();
    }
}
