// V0.63, V0.64 : ecrit sur la sortie quatre mois de meteo de synthese (novembre 2023 a fevrier 2024),
// au format d'Open-Meteo, pour essayer exemples/meteo/pluie.cl sans Internet :
//   fabriquer paris > meteo_48.85_2.35.csv
//   fabriquer ouest > meteo_48.11_-1.68.csv      (le meme temps, 6 heures plus tot : il vient de l'ouest)
// Des depressions passent (la pression baisse, les nuages et l'humidite montent, le vent tourne a l'ouest,
// il pleut parfois) ; deux heures de Paris n'ont pas de pression, comme il arrive dans les vrais fichiers.
import os
import math

def deux(n: int) -> string {
    if n < 10 {
        return "0" + str(n)
    }
    return str(n)
}

def main() -> int {
    let a: list[string] = os.args()
    let ouest: bool = len(a) > 0 and a[0] == "ouest"
    seed(5)
    let mois: list[int] = [11, 12, 1, 2]
    let annees: list[int] = [2023, 2023, 2024, 2024]
    let jours: list[int] = [30, 31, 31, 29]
    let dates: list[string] = []
    for m in range(0, 4) {
        for j in range(1, jours[m] + 1) {
            for h in range(0, 24) {
                dates.append(str(annees[m]) + "-" + deux(mois[m]) + "-" + deux(j) + "T" + deux(h) + ":00")
            }
        }
    }
    // le temps qui passe, 6 heures de plus que les dates (l'ouest a 6 heures d'avance)
    let n: int = len(dates) + 6
    let temp: list[float] = []
    let hum: list[float] = []
    let pres: list[float] = []
    let pluie: list[float] = []
    let nuages: list[float] = []
    let vent: list[float] = []
    let dir: list[float] = []
    let p: float = 1013.0
    let phase: float = 0.0
    let h_air: float = 80.0
    let c: float = 60.0
    let reste: int = 0
    for k in range(0, n) {
        let h: int = k % 24
        phase += 2.0 * math.pi / (24.0 * uniform(3.0, 6.0))
        p += 0.3 * math.cos(phase) + gauss(0.0, 0.15) - 0.002 * (p - 1013.0)
        c += 0.2 * (55.0 - 30.0 * math.cos(phase) + gauss(0.0, 10.0) - c)
        c = min(100.0, max(0.0, c))
        h_air += 0.15 * (75.0 + 0.25 * (c - 50.0) - h_air) + gauss(0.0, 1.5)
        if reste == 0 and random() < 0.12 * max(0.0, -math.cos(phase)) * (c / 100.0) * (c / 100.0) {
            reste = randint(2, 10)
        }
        let pl: float = 0.0
        if reste > 0 {
            pl = -0.9 * math.log(1.0 - random())
            reste -= 1
            h_air += 2.0
        }
        h_air = min(100.0, max(20.0, h_air))
        temp.append(6.0 + 3.0 * math.sin(2.0 * math.pi * float(h - 9) / 24.0) + gauss(0.0, 1.0))
        hum.append(h_air)
        pres.append(p)
        pluie.append(pl)
        nuages.append(c)
        vent.append(abs(12.0 - 8.0 * math.cos(phase) + gauss(0.0, 3.0)))
        let d: float = 250.0 + 70.0 * math.cos(phase) + gauss(0.0, 20.0)
        dir.append(d - 360.0 * math.floor(d / 360.0))
    }
    print("latitude,longitude,elevation,utc_offset_seconds,timezone,timezone_abbreviation")
    print("48.84,2.36,43.0,0,GMT,GMT")
    print("")
    print("time,temperature_2m (°C),relative_humidity_2m (%),pressure_msl (hPa),precipitation (mm),cloud_cover (%),wind_speed_10m (km/h),wind_direction_10m (°)")
    for k in range(0, len(dates)) {
        let i: int = k
        if ouest {
            i = k + 6
        }
        let pr: string = format(pres[i], ".1f")
        if not ouest and (k == 100 or k == 2000) {
            pr = ""
        }
        print(dates[k] + "," + format(temp[i], ".1f") + "," + format(hum[i], ".0f") + "," + pr + "," + format(pluie[i], ".2f") + "," + format(nuages[i], ".0f") + "," + format(vent[i], ".1f") + "," + format(dir[i], ".0f"))
    }
    return 0
}
