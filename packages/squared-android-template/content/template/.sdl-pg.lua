-- Package-validation compatibility marker; squared-pg removes this legacy
-- filename when rendering a new project.
return {
    format = 3,
    generator = "squared-pg",
    generator_version = "0.6.0-dev.5",
    name = "{{PROJECT_NAME}}",
    package = "{{PACKAGE_NAME}}",
    version = "{{BASE_VERSION}}",
    template = "dev.squarednetizen.template.android-sdl2-lua@0.6.0-dev.14"
}
