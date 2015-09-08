package com.example.rerun.app;

import java.util.Arrays;
import java.util.Date;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;
import eu.chainfire.libsuperuser.Shell;


public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Button button = (Button) findViewById(R.id.run);
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // http://themakeinfo.com/2015/05/building-android-root-app/
                if (Shell.SU.available()) {
                    Shell.SU.run(Arrays.asList(
                            "pkill res_mara",
                            "am force-stop jp.co.bandainamcoent.BNEI0242",
                            "cp /data/data/jp.co.bandainamcoent.BNEI0242/shared_prefs/jp.co.bandainamcoent.BNEI0242.xml /sdcard/jp.xml" + new Date().getTime(),
                            "rm /data/data/jp.co.bandainamcoent.BNEI0242/shared_prefs/jp.co.bandainamcoent.BNEI0242.xml"
                    ));
                    Intent launchIntent = getPackageManager().getLaunchIntentForPackage("jp.co.bandainamcoent.BNEI0242");
                    startActivity(launchIntent);
                    Shell.SU.run(Arrays.asList(
                            "cd /sdcard",
                            "nohup res_mara 1 &"
                    ));
                } else {
                    Toast.makeText(MainActivity.this, "SU is not available", Toast.LENGTH_LONG).show();
                }
            }
        });
    }
}
