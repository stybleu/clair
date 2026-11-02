// V0.63 : ecrit sur la sortie (fabriquer > historique.csv) quatre mois de meteo de synthese (novembre 2023 a fevrier 2024),
// au format d'Open-Meteo, pour essayer exemples/meteo/pluie.cl sans Internet.
// Des depressions passent (la pression baisse, les nuages et l'humidite montent, il pleut parfois) ;
// deux heures n'ont pas de pression, comme il arrive dans les vrais fichiers.
import math

def deux(n: int) -> string {
    if n < 10 {
        return "0" + str(n)
    }
    return str(n)
}

def main() -> int {
    seed(5)
    let mois: list[int] = [11, 12, 1, 2]
    let annees: list[int] = [2023, 2023, 2024, 2024]
    let jours: list[int] = [30, 31, 31, 29]
    print("latitude,longitude,elevation,utc_offset_seconds,timezone,timezone_abbreviation")
    print("48.84,2.36,43.0,3600,Europe/Paris,GMT+1")
    print("")
    print("time,temperature_2m (°C),relative_humidity_2m (%),pressure_msl (hPa),precipitation (mm),cloud_cover (%),wind_speed_10m (km/h)")
    let p: float = 1013.0
    let phase: float = 0.0
    let hum: float = 80.0
    let nuages: float = 60.0
    let reste: int = 0
    let k: int = 0
    for m in range(0, 4) {
        for j in range(1, jours[m] + 1) {
            for h in range(0, 24) {
                phase += 2.0 * math.pi / (24.0 * uniform(3.0, 6.0))
                p += 0.3 * math.cos(phase) + gauss(0.0, 0.15) - 0.002 * (p - 1013.0)
                let temp: float = 6.0 + 3.0 * math.sin(2.0 * math.pi * float(h - 9) / 24.0) + gauss(0.0, 1.0)
                nuages += 0.2 * (55.0 - 30.0 * math.cos(phase) + gauss(0.0, 10.0) - nuages)
                nuages = min(100.0, max(0.0, nuages))
                hum += 0.15 * (75.0 + 0.25 * (nuages - 50.0) - hum) + gauss(0.0, 1.5)
                if reste == 0 and random() < 0.12 * max(0.0, -math.cos(phase)) * (nuages / 100.0) * (nuages / 100.0) {
                    reste = randint(2, 10)
                }
                let pluie: float = 0.0
                if reste > 0 {
                    pluie = -0.9 * math.log(1.0 - random())
                    reste -= 1
                    hum += 2.0
                }
                hum = min(100.0, max(20.0, hum))
                let vent: float = abs(12.0 - 8.0 * math.cos(phase) + gauss(0.0, 3.0))
                let pres: string = format(p, ".1f")
                if k == 100 or k == 2000 {
                    pres = ""
                }
                print(str(annees[m]) + "-" + deux(mois[m]) + "-" + deux(j) + "T" + deux(h) + ":00," + format(temp, ".1f") + "," + format(hum, ".0f") + "," + pres + "," + format(pluie, ".2f") + "," + format(nuages, ".0f") + "," + format(vent, ".1f"))
                k += 1
            }
        }
    }
    return 0
}
