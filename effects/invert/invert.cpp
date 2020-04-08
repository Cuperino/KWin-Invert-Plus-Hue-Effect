/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2020 Javier Cordero <javier@imaginary.tech>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "invert.h"

#include <QAction>
#include <QFile>
#include <kwinglutils.h>
#include <kwinglplatform.h>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QStandardPaths>

#include <QMatrix4x4>

namespace KWin
{

InvertEffect::InvertEffect()
    :   m_inited(false),
        m_valid(true),
        m_invert_shader(nullptr),
        m_invert_plus_hue_shader(nullptr),
        m_allWindowsInvert(false),
        m_allWindowsInvertPlusHue(false)
{
    QAction* a = new QAction(this);
    a->setObjectName(QStringLiteral("Invert"));
    a->setText(i18n("Toggle Invert Effect"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_I);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_I);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::META + Qt::Key_I, a);
    connect(a, &QAction::triggered, this, &InvertEffect::toggleScreenInversion);

    QAction* b = new QAction(this);
    b->setObjectName(QStringLiteral("InvertWindow"));
    b->setText(i18n("Toggle Invert Effect on Window"));
    KGlobalAccel::self()->setDefaultShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_U);
    KGlobalAccel::self()->setShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_U);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::META + Qt::Key_U, b);
    connect(b, &QAction::triggered, this, &InvertEffect::toggleWindowInversion);

    QAction* c = new QAction(this);
    c->setObjectName(QStringLiteral("InvertPlusHue"));
    c->setText(i18n("Toggle Invert Plus Hue Effect"));
    KGlobalAccel::self()->setDefaultShortcut(c, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_H);
    KGlobalAccel::self()->setShortcut(c, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_H);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::META + Qt::Key_H, c);
    connect(c, &QAction::triggered, this, &InvertEffect::toggleScreenInversionPlusHue);

    QAction* d = new QAction(this);
    d->setObjectName(QStringLiteral("InvertPlusHueWindow"));
    d->setText(i18n("Toggle Invert Plus Hue Effect on Window"));
    KGlobalAccel::self()->setDefaultShortcut(d, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_G);
    KGlobalAccel::self()->setShortcut(d, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_G);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::META + Qt::Key_G, d);
    connect(d, &QAction::triggered, this, &InvertEffect::toggleWindowInversionPlusHue);

    connect(effects, &EffectsHandler::windowClosed, this, &InvertEffect::slotWindowClosed);
}

InvertEffect::~InvertEffect()
{
    delete m_invert_shader;
    delete m_invert_plus_hue_shader;
}

bool InvertEffect::supported()
{
    return effects->compositingType() == OpenGL2Compositing;
}

bool InvertEffect::loadData()
{
    m_inited = true;

    m_invert_shader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("invert.frag"));
    m_invert_plus_hue_shader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("invert-plus-hue.frag"));
    if (!m_invert_shader->isValid() || !m_invert_plus_hue_shader->isValid()) {
        qCCritical(KWINEFFECTS) << "The shaders failed to load!";
        return false;
    }

    return true;
}

void InvertEffect::drawWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data)
{
    // Load if we haven't already
    if (m_valid && !m_inited)
        m_valid = loadData();

    const bool invertWindow = m_windows_invert.contains(w);
    const bool invertPlusHueWindow = m_windows_invert_plus_hue.contains(w);
    bool useInvertShader = false;
    bool useInvertPlusHueShader = false;
    if (m_valid) {
        if (m_allWindowsInvert) {
            if (invertPlusHueWindow)
                useInvertPlusHueShader = true;
            else if (!invertWindow)
                useInvertShader = true;
        }
        else if (m_allWindowsInvertPlusHue) {
            if (invertWindow)
                useInvertShader = true;
            else if (!invertPlusHueWindow)
                useInvertPlusHueShader = true;
        }
        else {
            if (invertWindow)
                useInvertShader = true;
            else if (invertPlusHueWindow)
                useInvertPlusHueShader = true;
        }
    }
    if (useInvertShader) {
        ShaderManager *shaderManager = ShaderManager::instance();
        shaderManager->pushShader(m_invert_shader);

        data.shader = m_invert_shader;
    }
    else if (useInvertPlusHueShader) {
        ShaderManager *shaderManager = ShaderManager::instance();
        shaderManager->pushShader(m_invert_plus_hue_shader);

        data.shader = m_invert_plus_hue_shader;
    }

    effects->drawWindow(w, mask, region, data);

    if (useInvertShader || useInvertPlusHueShader) {
        ShaderManager::instance()->popShader();
    }
}

void InvertEffect::paintEffectFrame(KWin::EffectFrame* frame, const QRegion &region, double opacity, double frameOpacity)
{
    if (m_valid) {
        if (m_allWindowsInvert) {
            frame->setShader(m_invert_shader);
            ShaderBinder binder(m_invert_shader);
            effects->paintEffectFrame(frame, region, opacity, frameOpacity);
        }
        else if (m_allWindowsInvertPlusHue) {
            frame->setShader(m_invert_plus_hue_shader);
            ShaderBinder binder(m_invert_plus_hue_shader);
            effects->paintEffectFrame(frame, region, opacity, frameOpacity);
        }
    }
    else
        effects->paintEffectFrame(frame, region, opacity, frameOpacity);
}

void InvertEffect::slotWindowClosed(EffectWindow* w)
{
    m_windows_invert.removeOne(w);
    m_windows_invert_plus_hue.removeOne(w);
}

void InvertEffect::toggleWindowInversion()
{
    if (!effects->activeWindow()) {
        return;
    }
    if (!m_windows_invert.contains(effects->activeWindow())) {
        if (m_windows_invert_plus_hue.contains(effects->activeWindow()))
            m_windows_invert_plus_hue.removeOne(effects->activeWindow());
        m_windows_invert.append(effects->activeWindow());
    }
    else
        m_windows_invert.removeOne(effects->activeWindow());
    effects->activeWindow()->addRepaintFull();
}

void InvertEffect::toggleWindowInversionPlusHue()
{
    if (!effects->activeWindow()) {
        return;
    }
    if (!m_windows_invert_plus_hue.contains(effects->activeWindow())) {
        if (m_windows_invert.contains(effects->activeWindow()))
            m_windows_invert.removeOne(effects->activeWindow());
        m_windows_invert_plus_hue.append(effects->activeWindow());
    }
    else
        m_windows_invert_plus_hue.removeOne(effects->activeWindow());
    effects->activeWindow()->addRepaintFull();
}

void InvertEffect::toggleScreenInversion()
{
    m_allWindowsInvert = !m_allWindowsInvert;
    m_allWindowsInvertPlusHue = false;
    effects->addRepaintFull();
}

void InvertEffect::toggleScreenInversionPlusHue()
{
    m_allWindowsInvertPlusHue = !m_allWindowsInvertPlusHue;
    m_allWindowsInvert = false;
    effects->addRepaintFull();
}

bool InvertEffect::isActive() const
{
    return m_valid && (m_allWindowsInvert || m_allWindowsInvertPlusHue || !m_windows_invert.isEmpty() || !m_windows_invert_plus_hue.isEmpty());
}

bool InvertEffect::provides(Feature f)
{
    return f == ScreenInversion;
}

} // namespace

