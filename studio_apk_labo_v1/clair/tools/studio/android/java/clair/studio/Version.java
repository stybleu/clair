package clair.studio.labo;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;

/*
 * V0.71.7 : la version installee, demandee par Termux (fabriquer.sh) :
 *   am broadcast -n clair.studio/.Version -f 32
 * repond "Broadcast completed: result=-1, data="clair.studio 2"". Sans l'appli, il n'y a pas de data.
 * (pm list packages, qui servait avant, est refuse a Termux sur Android 16 : Failed transaction.)
 */
public class Version extends BroadcastReceiver {
    @Override
    public void onReceive(Context c, Intent i) {
        long v = 0;
        try {
            PackageInfo p = c.getPackageManager().getPackageInfo(c.getPackageName(), 0);
            v = p.getLongVersionCode();
        } catch (Exception e) {
            v = 0;
        }
        setResultData("clair.studio.labo " + v);
    }
}
