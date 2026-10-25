import json, urllib.request, sqlite3, sys

db = "samples/company_analytics.db"
con = sqlite3.connect(db)
cur = con.cursor()
schema = ""
for (name,) in cur.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"):
    cols = cur.execute(f"PRAGMA table_info({name})").fetchall()
    lines = []
    for c in cols:
        cid, cname, ctype, notnull, dflt, pk = c
        l = f"  {cname} {ctype or 'TEXT'}"
        if pk: l += " PRIMARY KEY"
        lines.append(l)
    schema += f"CREATE TABLE {name} (\n" + ",\n".join(lines) + "\n);\n"
    for fk in cur.execute(f"PRAGMA foreign_key_list({name})").fetchall():
        schema += f"-- FK: {name}.{fk[3]} -> {fk[2]}.{fk[4]}\n"
con.close()

Q = """Write ONE single SQLite query. HARD OUTPUT RULES: it MUST be at least 100 physical lines long; put every selected column, every CASE branch, every correlated subquery, every JOIN and every CTE clause on its OWN separate line; NEVER compress multiple things onto one line; fully expand all subqueries; no comments, no ellipsis, no truncation.

STRICT SCHEMA RULES (do NOT invent columns): reviews has ONLY (id, employee_id, quarter, rating) - there is NO department_id on reviews, so reach a department's reviews via reviews.employee_id = employees.id AND employees.department_id = departments.id. timesheets has ONLY (id, employee_id, project_id, week, hours) - also join through employees for a department. projects.status values are lowercase strings: 'active', 'done', 'on_hold'. For a monthly pivot write SUM(CASE WHEN STRFTIME('%m', x.week) = '01' THEN x.hours ELSE 0 END) directly (no CASE outside the aggregate).

Structure it as a WITH chain of CTEs, then a final SELECT over those CTEs, then a UNION ALL rollup row:

WITH
dept_salary AS (
  per department compute: headcount, SUM salary, AVG salary, MIN salary, MAX salary,
  plus a correlated subquery for the name of the highest-paid employee,
  a correlated subquery for the name of the lowest-paid employee,
  and a correlated subquery approximating the median salary (AVG of the middle rows is fine),
),
dept_reviews AS (
  per department compute: review count, AVG rating,
  plus a correlated subquery for the best (max) quarter rating value,
  plus a correlated subquery for the worst (min) quarter rating value,
),
dept_projects AS (
  per department compute, each as its OWN correlated subquery on separate lines:
  total spend of ACTIVE projects, total spend of DONE projects, total spend of ON_HOLD projects,
  count of active projects, count of done projects, AVG project spend,
  and the name of the single biggest project by spend,
),
dept_hours AS (
  per department compute: total logged hours from timesheets joined to employees,
  AVG hours per employee entry, and a FULL 12-month pivot:
  one SUM(t.hours) with CASE WHEN STRFTIME('%m', t.week) = '01' ... for each of the 12 months 01..12,
  each month on its own line,
),
emp_rank AS (
  every employee with RANK() OVER (PARTITION BY department_id ORDER BY salary DESC),
  DENSE_RANK() OVER (ORDER BY performance_score DESC),
  and a correlated subquery of how far below the department's top salary this employee is,
),
company_stats AS (
  a single row: company-wide AVG salary, AVG performance_score, total project spend, total logged hours
)
SELECT
  department name, region, and every metric column from dept_salary, dept_reviews, dept_projects, dept_hours
  joined on the department,
  plus each department's AVG salary as a percentage of company_stats AVG salary (separate lines),
  plus each department's total project spend as a percentage of company_stats total spend (separate lines),
  plus a CASE that buckets headcount into 'small' / 'medium' / 'large' with each WHEN on its own line,
  plus one extra correlated subquery picking the emp_rank name with the highest performance in that department
UNION ALL
SELECT
  a final 'ALL DEPARTMENTS (TOTAL)' rollup row aggregating the same columns across the whole company
ORDER BY 1;
"""

prompt = f"SQLite schema:\n{schema}\nQuestion (TR/EN): {Q}\n\nWrite one read-only SQL query answering the question."
body = {"model": "qwen2.5-coder:3b", "prompt": prompt, "stream": False,
        "system": "You are a SQLite SQL generator. Reply with exactly one read-only SQL statement inside a ```sql code fence. Write the complete query without truncation; expand every subquery fully even if it becomes very long. No prose outside the fence.",
        "options": {"temperature": 0.1, "num_predict": 8192}}
req = urllib.request.Request("http://localhost:11434/api/generate",
                             data=json.dumps(body).encode(), headers={"Content-Type": "application/json"})
r = json.load(urllib.request.urlopen(req, timeout=180))
resp = r["response"]
# extract sql between fences
import re
m = re.search(r"```sql\s*(.*?)```", resp, re.S)
sql = m.group(1).strip() if m else resp.strip()
lines = [ln for ln in sql.splitlines() if ln.strip()]
print(f"TOTAL physical lines in SQL: {len(sql.splitlines())} | non-empty: {len(lines)}")
print("----- SQL (first 4000 chars) -----")
print(sql[:4000])
# validate it runs read-only against sqlite
try:
    con = sqlite3.connect(db); con.execute("BEGIN")
    cur = con.execute(sql)
    rows = cur.fetchall()
    print(f"\n[EXECUTES OK] rows returned: {len(rows)}")
    con.rollback()
except Exception as e:
    print(f"\n[EXECUTION ERROR] {e}")
finally:
    try: con.close()
    except: pass
