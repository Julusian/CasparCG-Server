

#ifndef CASPARCG_SERVER_SCREEN_WINDOW_H
#define CASPARCG_SERVER_SCREEN_WINDOW_H

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QtGui/QWindow>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

class screen_window : public QWindow
{
    Q_OBJECT

  public:
    screen_window();
};

//! [1]
class SquircleRenderer
    : public QObject
    , protected QOpenGLFunctions
{
    Q_OBJECT
  public:
    ~SquircleRenderer();

    void setT(qreal t) { m_t = t; }
    void setViewportSize(const QSize& size) { m_viewportSize = size; }
    void setWindow(QQuickWindow* window) { m_window = window; }

  public slots:
    void init();
    void paint();

  private:
    QSize                 m_viewportSize;
    qreal                 m_t       = 0.0;
    QOpenGLShaderProgram* m_program = nullptr;
    QQuickWindow*         m_window  = nullptr;
};
//! [1]

//! [2]
class Squircle : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal t READ t WRITE setT NOTIFY tChanged)
    QML_ELEMENT

  public:
    Squircle();

    qreal t() const { return m_t; }
    void  setT(qreal t);

  signals:
    void tChanged();

  public slots:
    void sync();
    void cleanup();

  private slots:
    void handleWindowChanged(QQuickWindow* win);

  private:
    void releaseResources() override;

    qreal             m_t;
    SquircleRenderer* m_renderer;
};
//! [2]

#endif // CASPARCG_SERVER_SCREEN_WINDOW_H
