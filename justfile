set dotenv-load

export VERSION := env_var_or_default("VERSION", `date +'%Y%m%d.%H%M'`)

# Generate a version name (that can be used in follow up commands)
version:
    @echo "{{VERSION}}"

# Publish latest image to Cumulocity
publish:
    cd {{justfile_directory()}} && ./scripts/upload-c8y.sh

# Publish a given github release to Cumulocity (using external urls)
publish-external tag *args="":
    cd {{justfile_directory()}} && ./scripts/c8y-publish-release.sh {{tag}} {{args}}

# Trigger a release (by creating a tag)
release:
    git tag -a "{{VERSION}}" -m "{{VERSION}}"
    git push origin "{{VERSION}}"
    @echo
    @echo "Created release (tag): {{VERSION}}"
    @echo
