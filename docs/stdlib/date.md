# Módulo estándar `date`

El módulo `date` usa funciones externas C para fechas y calendario:

```rivo
import date

fn main():
    print(date.current_date())
    print(date.is_leap_year(2024))
```

API: `current_date`, `current_time`, `current_datetime`, `epoch_to_date`,
`date_to_epoch`, `add_days`, `days_between`, `is_leap_year`, `is_valid_date`,
`day_of_week`, `day_of_week_name`, `days_in_month` y `month_name`. Las
convenciones de zona horaria y formato dependen de `lib/c/date.c`.
