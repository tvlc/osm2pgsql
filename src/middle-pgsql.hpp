#ifndef OSM2PGSQL_MIDDLE_PGSQL_HPP
#define OSM2PGSQL_MIDDLE_PGSQL_HPP

/* Implements the mid-layer processing for osm2pgsql
 * using several PostgreSQL tables
 *
 * This layer stores data read in from the planet.osm file
 * and is then read by the backend processing code to
 * emit the final geometry-enabled output formats
*/

#include <memory>

#include "db-copy-mgr.hpp"
#include "id-tracker.hpp"
#include "middle.hpp"
#include "node-persistent-cache.hpp"
#include "node-ram-cache.hpp"
#include "pgsql.hpp"

class middle_query_pgsql_t : public middle_query_t
{
public:
    middle_query_pgsql_t(
        std::string const &conninfo,
        std::shared_ptr<node_ram_cache> const &cache,
        std::shared_ptr<node_persistent_cache> const &persistent_cache);

    size_t nodes_get_list(osmium::WayNodeList *nodes) const override;

    bool way_get(osmid_t id, osmium::memory::Buffer &buffer) const override;
    size_t rel_way_members_get(osmium::Relation const &rel, rolelist_t *roles,
                               osmium::memory::Buffer &buffer) const override;

    idlist_t relations_using_way(osmid_t way_id) const override;
    bool relation_get(osmid_t id,
                      osmium::memory::Buffer &buffer) const override;

    void exec_sql(std::string const &sql_cmd) const;

private:
    size_t local_nodes_get_list(osmium::WayNodeList *nodes) const;

    pg_result_t exec_prepared(char const *stmt, char const *param) const;
    pg_result_t exec_prepared(char const *stmt, osmid_t osm_id) const;

    pg_conn_t m_sql_conn;
    std::shared_ptr<node_ram_cache> m_cache;
    std::shared_ptr<node_persistent_cache> m_persistent_cache;
};

struct table_sql {
    char const *name = "";
    char const *create_table = "";
    char const *prepare_query = "";
    char const *prepare_mark = "";
    char const *create_index = "";
};

struct middle_pgsql_t : public slim_middle_t
{
    middle_pgsql_t(options_t const *options);

    void start() override;
    void stop(osmium::thread::Pool &pool) override;
    void analyze() override;
    void commit() override;

    void node_set(osmium::Node const &node) override;
    void node_delete(osmid_t id) override;
    void node_changed(osmid_t id) override;

    void way_set(osmium::Way const &way) override;
    void way_delete(osmid_t id) override;
    void way_changed(osmid_t id) override;

    void relation_set(osmium::Relation const &rel) override;
    void relation_delete(osmid_t id) override;
    void relation_changed(osmid_t id) override;

    void flush() override;

    void iterate_ways(middle_t::pending_processor &pf) override;
    void iterate_relations(pending_processor &pf) override;

    size_t pending_count() const override;

    class table_desc
    {
    public:
        table_desc() {}
        table_desc(options_t const &options, table_sql const &ts);

        char const *name() const { return m_copy_target->name.c_str(); }
        void clear_array_indexes() { m_array_indexes.clear(); }

        void stop(std::string const &conninfo, bool droptemp,
                  bool build_indexes);

        std::string m_create;
        std::string m_prepare_query;
        std::string m_prepare_intarray;
        std::string m_array_indexes;

        std::shared_ptr<db_target_descr_t> m_copy_target;
    };

    std::shared_ptr<middle_query_t> get_query_instance() override;

private:
    enum middle_tables
    {
        NODE_TABLE = 0,
        WAY_TABLE,
        REL_TABLE,
        NUM_TABLES
    };

    void buffer_store_tags(osmium::OSMObject const &obj, bool attrs);
    pg_result_t exec_prepared(char const *stmt, osmid_t osm_id) const;

    table_desc m_tables[NUM_TABLES];

    bool m_append;
    bool m_mark_pending;
    options_t const *m_out_options;

    std::shared_ptr<node_ram_cache> m_cache;
    std::shared_ptr<node_persistent_cache> m_persistent_cache;

    std::shared_ptr<id_tracker> m_ways_pending_tracker, m_rels_pending_tracker;

    std::unique_ptr<pg_conn_t> m_query_conn;
    // middle keeps its own thread for writing to the database.
    std::shared_ptr<db_copy_thread_t> m_copy_thread;
    db_copy_mgr_t<db_deleter_by_id_t> m_db_copy;
};

#endif // OSM2PGSQL_MIDDLE_PGSQL_HPP
