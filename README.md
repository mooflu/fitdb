## Notes to self

- use [gcexport.py](https://github.com/pe-st/garmin-connect-export) to grab FIT files from Garmin site.
```
python3 gcexport.py --username <username> -f original -u -c all
if some of the original files are gpx, grab them as tcx
python3 gcexport.py --username <username> -f tcx -u -c all
```
- use [gpsbabel](https://github.com/GPSBabel/gpsbabel) to convert non FIT to FIT
```
for x in *.tcx; do ../../gpsbabel/gpsbabel -i gtrnctr -f $x -o garmin_fit -F ${x:r}.fit ; done
```
- build fitdb
- import data
```
./build/src/fitdb -i ../fit_data
```
- query by location and date range
```
./build/src/fitdb --nec 49.789256859574444 -122.98610951567522 --swc 49.7830224018896 -122.9937909886709 -f 2011-01-01 -t 2015-01-01
query:
select distinct(a.garminActivity) from activity a, location l on a.id=l.activityId
where minlat>593934591.9781978 and maxLat<594008971.9555998 and minLng>-1467373083.076114 and maxLng<-1467281439.5336096 and startTime>=662716800 and startTime<=788947200
order by a.garminActivity desc
limit 100;

Results:
https://connect.garmin.com/modern/activity/<...>
...
```