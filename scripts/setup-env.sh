#!/bin/bash
# Setup script for EthervoxAI development environment

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║         EthervoxAI Development Environment Setup          ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Check if .env exists
if [ -f ".env" ]; then
    echo "✓ .env file already exists"
    read -p "Do you want to overwrite it? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Keeping existing .env file"
        exit 0
    fi
fi

# Copy .env.example to .env
if [ ! -f ".env.example" ]; then
    echo "✗ Error: .env.example not found"
    exit 1
fi

cp .env.example .env
echo "✓ Created .env file from template"

# Prompt for GitHub token
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  GitHub Token Setup (for bug reporting)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "You can configure separate tokens for Android and Desktop platforms,"
echo "or use a single token for both."
echo ""
echo "Instructions:"
echo "  1. Go to: https://github.com/settings/tokens?type=beta"
echo "  2. Generate fine-grained token(s)"
echo "  3. Give 'Issues: Read and write' permission"
echo "  4. Scope to appropriate repository"
echo ""
read -p "Do you want to set up bug reporting now? (y/N): " -n 1 -r
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "Platform-specific tokens (recommended for security):"
    echo ""
    
    # Android token
    read -p "Enter Android token (or leave empty): " -s android_token
    echo ""
    
    # Desktop token
    read -p "Enter Desktop token (or leave empty): " -s desktop_token
    echo ""
    
    # Generic token
    read -p "Enter generic fallback token (or leave empty): " -s generic_token
    echo ""
    
    # Update .env file
    if [ -n "$android_token" ]; then
        if [[ "$OSTYPE" == "darwin"* ]]; then
            sed -i '' "s/ETHERVOX_GITHUB_TOKEN_ANDROID=.*/ETHERVOX_GITHUB_TOKEN_ANDROID=$android_token/" .env
        else
            sed -i "s/ETHERVOX_GITHUB_TOKEN_ANDROID=.*/ETHERVOX_GITHUB_TOKEN_ANDROID=$android_token/" .env
        fi
        echo "✓ Android token saved"
    fi
    
    if [ -n "$desktop_token" ]; then
        if [[ "$OSTYPE" == "darwin"* ]]; then
            sed -i '' "s/ETHERVOX_GITHUB_TOKEN_DESKTOP=.*/ETHERVOX_GITHUB_TOKEN_DESKTOP=$desktop_token/" .env
        else
            sed -i "s/ETHERVOX_GITHUB_TOKEN_DESKTOP=.*/ETHERVOX_GITHUB_TOKEN_DESKTOP=$desktop_token/" .env
        fi
        echo "✓ Desktop token saved"
    fi
    
    if [ -n "$generic_token" ]; then
        if [[ "$OSTYPE" == "darwin"* ]]; then
            sed -i '' "s/ETHERVOX_GITHUB_TOKEN=.*/ETHERVOX_GITHUB_TOKEN=$generic_token/" .env
        else
            sed -i "s/ETHERVOX_GITHUB_TOKEN=.*/ETHERVOX_GITHUB_TOKEN=$generic_token/" .env
        fi
        echo "✓ Generic token saved"
    fi
    
    if [ -z "$android_token" ] && [ -z "$desktop_token" ] && [ -z "$generic_token" ]; then
        echo "⚠ No tokens provided - bug reporting will be disabled"
    fi
else
    echo "⚠ Skipping GitHub token setup - bug reporting will be disabled"
    echo "  You can add tokens later by editing .env"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Next Steps"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "To load environment variables for your shell session:"
echo ""
echo "  source <(grep -v '^#' .env | sed 's/^/export /')"
echo ""
echo "Or add to your shell profile (~/.zshrc or ~/.bashrc):"
echo ""
echo "  export ETHERVOX_GITHUB_TOKEN=\"your_token_here\""
echo ""
echo "✓ Setup complete!"
