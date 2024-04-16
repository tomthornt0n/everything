
# hacky little script to create dummy invoices per invoiced date in the old db

import sqlite3

con = sqlite3.connect("ptbd_timesheets.db")

cur = con.cursor()

timesheets = cur.execute("select * from timesheets").fetchall()
disbursements = cur.execute("select * from disbursements").fetchall()

invoice_dates = {}

number = 1

for timesheet in timesheets:
    if timesheet[7] is not None:
        if timesheet[7] not in invoice_dates:
            print(f"inserting invoice {number} for date {timesheet[7]}")
            cur.execute(f"insert into invoices (address, title, description, number, date) values('placeholder', 'placeholder', 'placeholder', {number}, {timesheet[7]})")
            invoice_dates[timesheet[7]] = number
            number += 1
        cur.execute(f"update timesheets set invoice = {invoice_dates[timesheet[7]]} where id = {timesheet[0]}")

for disbursement in disbursements:
    if disbursement[5] is not None:
        if disbursement[5] not in invoice_dates:
            print(f"inserting invoice {number} for date {disbursement[5]}")
            cur.execute(f"insert into invoices (address, title, description, number, date) values('placeholder', 'placeholder', 'placeholder', {number}, {disbursement[5]})")
            invoice_dates[disbursement[5]] = number
            number += 1
        cur.execute(f"update disbursements set invoice = {invoice_dates[disbursement[5]]} where id = {disbursement[0]}")

con.commit()
