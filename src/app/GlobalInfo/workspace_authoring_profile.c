#include "app/GlobalInfo/workspace_authoring_profile.h"

#include <limits.h>
#include <string.h>

#include "core_pack.h"
#include "core_pane_module.h"
#include "core_pane_snapshot.h"

#define IDE_WAPP_PAYLOAD_SIZE 16u
#define IDE_WAPP_REQUIREMENT_SIZE 12u
#define IDE_WAPP_REQUIREMENT_COUNT IDE_WORKSPACE_AUTHORING_MODULE_COUNT

static void put16(unsigned char *p, uint16_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8u); }
static void put32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8u);
    p[2] = (unsigned char)(v >> 16u); p[3] = (unsigned char)(v >> 24u);
}
static uint16_t get16(const unsigned char *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8u)); }
static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static void make_requirements(CorePaneWorkspaceProfileModuleRequirementV1 *requirements) {
    uint32_t i;
    for (i = 0u; i < IDE_WAPP_REQUIREMENT_COUNT; ++i) {
        requirements[i] = (CorePaneWorkspaceProfileModuleRequirementV1){
            ide_workspace_authoring_projection_module_type_id((IDEWorkspaceAuthoringBuiltinModule)i),
            1u, 0u, 1u, 0u};
    }
}

static IDEWorkspaceAuthoringProfileResult validate_compatibility(
    const CorePaneWorkspaceProfileModuleRequirementV1 *requirements) {
    static const char *const keys[IDE_WAPP_REQUIREMENT_COUNT] = {
        "ide_tool_project", "ide_tool_libraries", "ide_tool_build_output", "ide_tool_errors",
        "ide_tool_assets", "ide_tool_tasks", "ide_tool_version_control"};
    CorePaneModuleDescriptor entries[IDE_WAPP_REQUIREMENT_COUNT];
    CorePaneModuleRegistry registry;
    CorePaneModuleProfileRequirement local[IDE_WAPP_REQUIREMENT_COUNT];
    CorePaneSnapshotNodeRecordV1 node = {0u, 1u, CORE_PANE_SNAPSHOT_NODE_LEAF,
        CORE_PANE_SNAPSHOT_AXIS_HORIZONTAL, 0u, 0.0f, UINT32_MAX, UINT32_MAX, 0.0f, 0.0f};
    CorePaneWorkspaceProfileV1 envelope = {0};
    uint32_t i;

    if (!requirements || core_pane_module_registry_init(&registry, entries, IDE_WAPP_REQUIREMENT_COUNT) !=
                             CORE_PANE_MODULE_OK) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_REQUIREMENTS;
    for (i = 0u; i < IDE_WAPP_REQUIREMENT_COUNT; ++i) {
        CorePaneModuleDescriptor descriptor = {
            ide_workspace_authoring_projection_module_type_id((IDEWorkspaceAuthoringBuiltinModule)i),
            keys[i], keys[i], 1u, 0u, 1u, 0u, 0u, 0u,
            CORE_PANE_MODULE_PROVIDER_INTERNAL, NULL, NULL, NULL};
        if (core_pane_module_register(&registry, &descriptor) != CORE_PANE_MODULE_OK) {
            return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_REQUIREMENTS;
        }
        local[i] = (CorePaneModuleProfileRequirement){requirements[i].module_type_id,
            requirements[i].min_version_major, requirements[i].min_version_minor,
            requirements[i].state_schema_major, requirements[i].state_schema_minor};
    }
    if (core_pane_module_validate_profile_requirements(&registry, local, IDE_WAPP_REQUIREMENT_COUNT) !=
        CORE_PANE_MODULE_OK) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_REQUIREMENTS;

    memcpy(envelope.meta.host_id, "ide", sizeof("ide"));
    envelope.meta.schema_major = CORE_PANE_WORKSPACE_PROFILE_SCHEMA_MAJOR_V1;
    envelope.meta.schema_minor = CORE_PANE_WORKSPACE_PROFILE_SCHEMA_MINOR_V1;
    envelope.meta.host_version_major = IDE_WORKSPACE_AUTHORING_PROFILE_SCHEMA_MAJOR;
    envelope.meta.host_version_minor = IDE_WORKSPACE_AUTHORING_PROFILE_SCHEMA_MINOR;
    envelope.meta.module_requirement_count = IDE_WAPP_REQUIREMENT_COUNT;
    envelope.snapshot.meta = (CorePaneSnapshotMetaV1){CORE_PANE_SNAPSHOT_SCHEMA_MAJOR_V1,
        CORE_PANE_SNAPSHOT_SCHEMA_MINOR_V1, 0u, 0u, 0u, 0u, 1u, 0u, 0u};
    envelope.snapshot.nodes = &node;
    envelope.module_requirements = requirements;
    return core_pane_workspace_profile_validate_v1(&envelope) == CORE_PANE_SNAPSHOT_OK
               ? IDE_WORKSPACE_AUTHORING_PROFILE_OK : IDE_WORKSPACE_AUTHORING_PROFILE_ERR_SCHEMA;
}

IDEWorkspaceAuthoringProfileResult ide_workspace_authoring_profile_export_file(
    const char *path, const IDEWorkspaceAuthoringProjection *projection) {
    unsigned char payload[IDE_WAPP_PAYLOAD_SIZE] = {0};
    unsigned char requirement_bytes[IDE_WAPP_REQUIREMENT_COUNT * IDE_WAPP_REQUIREMENT_SIZE] = {0};
    CorePaneWorkspaceProfileModuleRequirementV1 requirements[IDE_WAPP_REQUIREMENT_COUNT];
    CorePackWriter writer;
    CoreResult result;
    uint32_t i;
    if (!path || !ide_workspace_authoring_projection_valid(projection)) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_INVALID_ARG;
    make_requirements(requirements);
    if (validate_compatibility(requirements) != IDE_WORKSPACE_AUTHORING_PROFILE_OK) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_REQUIREMENTS;
    put16(payload, IDE_WORKSPACE_AUTHORING_PROFILE_SCHEMA_MAJOR);
    put16(payload + 2u, IDE_WORKSPACE_AUTHORING_PROFILE_SCHEMA_MINOR);
    payload[4] = projection->tool_panel_visible ? 1u : 0u;
    payload[5] = projection->control_panel_visible ? 1u : 0u;
    payload[6] = projection->terminal_visible ? 1u : 0u;
    put32(payload + 8u, (uint32_t)projection->active_tool);
    put16(payload + 12u, IDE_WAPP_REQUIREMENT_COUNT);
    for (i = 0u; i < IDE_WAPP_REQUIREMENT_COUNT; ++i) {
        unsigned char *record = requirement_bytes + i * IDE_WAPP_REQUIREMENT_SIZE;
        put32(record, requirements[i].module_type_id); put16(record + 4u, requirements[i].min_version_major);
        put16(record + 6u, requirements[i].min_version_minor); put16(record + 8u, requirements[i].state_schema_major);
        put16(record + 10u, requirements[i].state_schema_minor);
    }
    result = core_pack_writer_open(path, &writer);
    if (result.code != CORE_OK) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_IO;
    result = core_pack_writer_add_chunk(&writer, "IWAP", payload, sizeof(payload));
    if (result.code == CORE_OK) result = core_pack_writer_add_chunk(&writer, "IMOD", requirement_bytes, sizeof(requirement_bytes));
    if (core_pack_writer_close(&writer).code != CORE_OK && result.code == CORE_OK) result.code = CORE_ERR_IO;
    return result.code == CORE_OK ? IDE_WORKSPACE_AUTHORING_PROFILE_OK : IDE_WORKSPACE_AUTHORING_PROFILE_ERR_IO;
}

IDEWorkspaceAuthoringProfileResult ide_workspace_authoring_profile_import_file(
    const char *path, IDEWorkspaceAuthoringProjection *out_projection) {
    CorePackReader reader;
    CorePackChunkInfo payload_chunk, requirement_chunk;
    unsigned char payload[IDE_WAPP_PAYLOAD_SIZE];
    unsigned char bytes[IDE_WAPP_REQUIREMENT_COUNT * IDE_WAPP_REQUIREMENT_SIZE];
    CorePaneWorkspaceProfileModuleRequirementV1 requirements[IDE_WAPP_REQUIREMENT_COUNT];
    IDEWorkspaceAuthoringProjection decoded;
    uint32_t i;
    if (!path || !out_projection) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_INVALID_ARG;
    if (core_pack_reader_open(path, &reader).code != CORE_OK) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_CONTAINER;
    if (core_pack_reader_chunk_count(&reader) != 2u ||
        core_pack_reader_find_chunk(&reader, "IWAP", 0u, &payload_chunk).code != CORE_OK ||
        core_pack_reader_find_chunk(&reader, "IMOD", 0u, &requirement_chunk).code != CORE_OK ||
        payload_chunk.size != sizeof(payload) || requirement_chunk.size != sizeof(bytes) ||
        core_pack_reader_read_chunk_data(&reader, &payload_chunk, payload, sizeof(payload)).code != CORE_OK ||
        core_pack_reader_read_chunk_data(&reader, &requirement_chunk, bytes, sizeof(bytes)).code != CORE_OK) {
        (void)core_pack_reader_close(&reader); return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_CONTAINER;
    }
    (void)core_pack_reader_close(&reader);
    if (get16(payload) != IDE_WORKSPACE_AUTHORING_PROFILE_SCHEMA_MAJOR ||
        get16(payload + 2u) != IDE_WORKSPACE_AUTHORING_PROFILE_SCHEMA_MINOR || payload[4] > 1u ||
        payload[5] > 1u || payload[6] > 1u || payload[7] != 0u ||
        get16(payload + 12u) != IDE_WAPP_REQUIREMENT_COUNT || get16(payload + 14u) != 0u) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_SCHEMA;
    decoded = (IDEWorkspaceAuthoringProjection){payload[4] != 0u, payload[5] != 0u, payload[6] != 0u,
        (IconTool)get32(payload + 8u)};
    if (!ide_workspace_authoring_projection_valid(&decoded)) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_PROJECTION;
    for (i = 0u; i < IDE_WAPP_REQUIREMENT_COUNT; ++i) {
        const unsigned char *record = bytes + i * IDE_WAPP_REQUIREMENT_SIZE;
        requirements[i] = (CorePaneWorkspaceProfileModuleRequirementV1){get32(record), get16(record + 4u),
            get16(record + 6u), get16(record + 8u), get16(record + 10u)};
    }
    if (validate_compatibility(requirements) != IDE_WORKSPACE_AUTHORING_PROFILE_OK) return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_REQUIREMENTS;
    *out_projection = decoded;
    return IDE_WORKSPACE_AUTHORING_PROFILE_OK;
}

const char *ide_workspace_authoring_profile_result_string(IDEWorkspaceAuthoringProfileResult result) {
    static const char *const names[] = {"ok", "invalid_arg", "io", "container", "schema", "requirements", "projection"};
    return result >= IDE_WORKSPACE_AUTHORING_PROFILE_OK && result <= IDE_WORKSPACE_AUTHORING_PROFILE_ERR_PROJECTION
               ? names[result] : "unknown";
}
