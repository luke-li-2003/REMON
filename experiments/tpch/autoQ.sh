function generate_tpch_queries() {
    local sf=${1:-1}
    local seed=${2:-42}

    mkdir -p queries
    rm -f queries/*.sql

    for q in $(seq 1 22); do
        echo "Generating Q${q}..."

        DSS_QUERY=queries_clean ./qgen \
            -s "$sf" \
            -r "$seed" \
            -N \
            "$q" > "queries/${q}.sql"

        # DuckDB compatibility fixes
        sed -i -E 's/day \([0-9]+\)/day/g' "queries/${q}.sql"

        # Normalize line endings
        dos2unix "queries/${q}.sql" >/dev/null 2>&1
    done

    echo "Generated and adapted TPC-H queries (SF=${sf}, seed=${seed})"
}

generate_tpch_queries $1 $2
