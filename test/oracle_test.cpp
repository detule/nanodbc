#include "test_case_fixture.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <atlsafe.h>
#include <comutil.h>
#ifdef _DEBUG
#pragma comment(lib, "comsuppwd.lib")
#else
#pragma comment(lib, "comsuppw.lib")
#endif
#endif

namespace
{
struct oracle_fixture : public test_case_fixture
{
    oracle_fixture()
        : test_case_fixture()
    {
        // connection string from command line or NANODBC_TEST_CONNSTR environment variable
        if (connection_string_.empty())
            connection_string_ = get_env("NANODBC_TEST_CONNSTR_ORACLE");
    }

    inline bool success(RETCODE rc)
    {
#ifdef NANODBC_ODBC_API_DEBUG
        std::cerr << "<-- rc: " << return_code(rc) << " | " << std::endl;
#endif
        return rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO;
    }

    // `name` is a type name
    // `def` is a comma separated column definitions, trailing '(' and ')' are optional.
    void create_table_type(
        nanodbc::connection& connection,
        nanodbc::string const& name,
        nanodbc::string def) const
    {
        nanodbc::string sql(NANODBC_TEXT("CREATE TYPE "));
        sql += name;
        sql += NANODBC_TEXT(" AS TABLE ");
        sql += def;
        sql += NANODBC_TEXT(';');

        drop_table_type(connection, name);
        execute(connection, sql);
    }

    virtual void drop_table_type(nanodbc::connection& connection, nanodbc::string const& name) const
    {
        bool type_exists = true;

        try
        {
            auto sql =
                NANODBC_TEXT("SELECT 1 FROM sys.types WHERE is_table_type = 1 AND name = '") +
                name + NANODBC_TEXT("';");
            nanodbc::result results = execute(connection, sql);
            results.next();
            type_exists = (0 < results.rows());
        }
        catch (...)
        {
            type_exists = false;
        }

        if (type_exists)
        {
            execute(connection, NANODBC_TEXT("DROP TYPE ") + name + NANODBC_TEXT(";"));
        }
    }
};
} // namespace

TEST_CASE_METHOD(oracle_fixture, "test_bind_date_to_datetime", "[mssql][bind][datetime]")
{
    auto conn = connect();
    create_table(
        conn,
        NANODBC_TEXT("test_bind_date_to_datetime"),
        NANODBC_TEXT("(vch varchar2(256 CHAR), dtm date)"));

    nanodbc::statement stmt(conn);
    prepare(stmt, NANODBC_TEXT("insert into test_bind_date_to_datetime(vch, dtm) values (?,?)"));

    std::vector<uint8_t> nulls(2, false);
    std::vector<std::string> strings;
    strings.emplace_back("TEST1");
    strings.emplace_back("TEST2");
    stmt.bind_strings(0, strings, reinterpret_cast<bool*>(nulls.data()));


    std::vector<nanodbc::date> dates;
    dates.emplace_back(nanodbc::date({.year = 2025, .month = 11, .day = 4}));
    dates.emplace_back(nanodbc::date({.year = 2025, .month = 11, .day = 5}));
    stmt.bind(1, dates.data(), 2, reinterpret_cast<bool*>(nulls.data()));

    nanodbc::execute(stmt, 2);
    {
        auto result =
            nanodbc::execute(conn, NANODBC_TEXT("select vch, dtm from test_bind_date_to_datetime"));
        result.next();
        REQUIRE(result.get<std::string>(0) == "TEST1");
        REQUIRE(result.get<nanodbc::date>(1).year == dates[0].year);
        REQUIRE(result.get<nanodbc::date>(1).month == dates[0].month);
        REQUIRE(result.get<nanodbc::date>(1).day == dates[0].day);
    }
}
