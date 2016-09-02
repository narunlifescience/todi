/* This file is part of Todi.

   Copyright 2016, Arun Narayanankutty <n.arun.lifescience@gmail.com>

   Todi is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   Todi is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with Todi.  If not, see <http://www.gnu.org/licenses/>.

   Description : Main player
*/

#include "Player.h"
#include "IconLoader.h"
#include "ui_Player.h"

#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSizeGrip>
#include <QSpacerItem>
#include <QTimer>
#include <QWidget>

#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTimer>
#include <QWheelEvent>
#include <memory>

#include "MpdConnectionDialog.h"
#include "AboutDialog.h"
#include "globals.h"
#include "lib/mpdparseutils.h"
#include "lib/mpdstats.h"
#include "lib/mpdstatus.h"
#include "musiclibraryitemsong.h"
#include "statistics_dialog.h"

Player::Player(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                          Qt::SubWindow),
      ui_(new Ui_Player),
      lastState(MPDStatus::State::StateInactive),
      lastSongId(-1),
      fetchStatsFactor(0),
      nowPlayingFactor(0),
      draggingPositionSlider(false),
      widget(new QWidget(this)),
      close_pushButton(new QPushButton(this)),
      expand_collapse_PushButton(new QPushButton(this)),
      previous_pushButton(new QPushButton(this)),
      play_pause_pushButton(new QPushButton(this)),
      next_pushButton(new QPushButton(this)),
      volume_pushButton(new QPushButton(this)),
      search_pushButton(new QPushButton(this)),
      playlist_pushButton(new QPushButton(this)),
      track_slider(new TrackSlider(Qt::Horizontal, this)),
      timer_label(new QLabel(this)),
      albumcover_label(new QLabel(this)),
      songMetadata_label(new QLabel(this)),
      volume_slider_frame(new QFrame(this, Qt::Popup)),
      volume_slider(new QSlider(Qt::Horizontal, volume_slider_frame)),
      resize_status(false) {
  ui_->setupUi(this);
  this->resize(this->width(), 61);

  IconLoader::lumen_ = IconLoader::isLight(Qt::black);
  this->setAttribute(Qt::WA_TranslucentBackground, true);
  setWindowTitle(tr("Todi"));
  setWindowIcon(QIcon(":icons/todi.svg"));

  QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
  effect->setColor(QColor(181, 185, 190));
  effect->setBlurRadius(5);
  effect->setXOffset(0);
  effect->setYOffset(0);
  setGraphicsEffect(effect);
  this->setGraphicsEffect(effect);
  qInfo() << "Starting Todi application...";

  // Set icons
  close_pushButton->setIcon(
      IconLoader::load("edit-close", IconLoader::LightDark));
  expand_collapse_PushButton->setIcon(
      IconLoader::load("edit-expand", IconLoader::LightDark));
  previous_pushButton->setIcon(
      IconLoader::load("media-skip-backward", IconLoader::LightDark));
  play_pause_pushButton->setIcon(
      IconLoader::load("media-playback-start", IconLoader::LightDark));
  next_pushButton->setIcon(
      IconLoader::load("media-skip-forward", IconLoader::LightDark));
  volume_pushButton->setIcon(
      IconLoader::load("media-playback-volume", IconLoader::LightDark));
  search_pushButton->setIcon(
      IconLoader::load("edit-find", IconLoader::LightDark));
  playlist_pushButton->setIcon(
      IconLoader::load("view-media-playlist", IconLoader::LightDark));

  // Right click context menu
  QAction *quitAction = new QAction(tr("&Exit"), this);
  QAction *about = new QAction(tr("&About"), this);
  addAction(quitAction);
  addAction(about);
  setContextMenuPolicy(Qt::ActionsContextMenu);

  // Theme adjust acordingly
  widget->setStyleSheet(
      ".QWidget{border-radius: 3px; background-color: rgba(20, 20, 20, 200); "
      "border: 0px solid #5c5c5c;}");
  track_slider->setStyleSheet(
      ".QSlider::groove:horizontal { border: 0px solid #999999; height: 3px; "
      "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, "
      "stop:1 #c4c4c4); "
      "margin: 0px 0; } .QSlider::handle:horizontal {"
      "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #eff0f1, "
      "stop:1 #eff0f1);"
      "border: 0px solid #5c5c5c;width: 2px;margin: -2px 0; }"
      ".QSlider::sub-page:horizontal {"
      "background: qlineargradient(x1: 0, y1: 0,    x2: 0, y2: 1,"
      "stop: 0 #3daee9, stop: 1 #3daee9); width: 3px; margin: 0px 0;}");
  close_pushButton->setStyleSheet("QPushButton{border: none;}");
  expand_collapse_PushButton->setStyleSheet("QPushButton{border: none;}");
  previous_pushButton->setStyleSheet("QPushButton{border: none;}");
  play_pause_pushButton->setStyleSheet("QPushButton{border: none;}");
  next_pushButton->setStyleSheet("QPushButton{border: none;}");
  volume_pushButton->setStyleSheet("QPushButton{border: none;}");
  search_pushButton->setStyleSheet("QPushButton{border: none;}");
  playlist_pushButton->setStyleSheet("QPushButton{border: none;}");
  timer_label->setStyleSheet("QLabel{color:rgba(200, 200, 200, 200)}");

  QGridLayout *baselayout = new QGridLayout(this);
  baselayout->addWidget(widget);

  QVBoxLayout *closecollapselayout = new QVBoxLayout;
  closecollapselayout->setContentsMargins(0, 3, 0, 3);
  closecollapselayout->addWidget(close_pushButton);
  closecollapselayout->addStretch();
  closecollapselayout->addWidget(expand_collapse_PushButton);

  QHBoxLayout *playerbuttonslayout = new QHBoxLayout;
  playerbuttonslayout->addWidget(previous_pushButton);
  playerbuttonslayout->addWidget(play_pause_pushButton);
  playerbuttonslayout->addWidget(next_pushButton);

  QHBoxLayout *slidertimerlayout = new QHBoxLayout;
  slidertimerlayout->setContentsMargins(0, 0, 0, 3);
  slidertimerlayout->setSpacing(3);
  slidertimerlayout->addWidget(track_slider);
  slidertimerlayout->addWidget(timer_label);

  QHBoxLayout *layout = new QHBoxLayout();
  layout->setContentsMargins(0, 3, 0, 0);
  layout->addStretch();
  layout->addLayout(playerbuttonslayout);
  layout->addStretch();
  layout->addWidget(volume_pushButton);
  layout->addWidget(search_pushButton);
  layout->addWidget(playlist_pushButton);

  QVBoxLayout *vboxplaypauseslidertimer = new QVBoxLayout();
  vboxplaypauseslidertimer->addLayout(layout);
  vboxplaypauseslidertimer->addLayout(slidertimerlayout);

  QHBoxLayout *hboxfinal = new QHBoxLayout(widget);
  // hboxfinal->setContentsMargins(3, 3, 0, 3);
  hboxfinal->setContentsMargins(0, 0, 5, 0);
  hboxfinal->setSpacing(5);
  // hboxfinal->addWidget(new QSizeGrip(widget), 0, Qt::AlignBottom |
  // Qt::AlignLeft);
  hboxfinal->addLayout(closecollapselayout);
  hboxfinal->addWidget(albumcover_label);
  hboxfinal->addWidget(songMetadata_label);
  hboxfinal->addLayout(vboxplaypauseslidertimer);
  // hboxfinal->addWidget(new QSizeGrip(widget), 0, Qt::AlignBottom |
  // Qt::AlignRight);
  songMetadata_label->setVisible(false);
  songMetadata_label->setText("test Todi compact");

  widget->layout()->setSizeConstraint(QLayout::SetNoConstraint);
  timer_label->setText("--:--");
  QPixmap pixmap(":/icons/nocover.svg");
  albumcover_label->setFixedWidth(this->height() - 20);
  albumcover_label->setFixedHeight(this->height() - 20);
  albumcover_label->resize(this->height() - 20, this->height() - 20);
  albumcover_label->setPixmap(
      pixmap.scaled(albumcover_label->size(), Qt::KeepAspectRatio));
  albumcover_label->setPixmap(pixmap.scaled(albumcover_label->size(),
                                            Qt::KeepAspectRatioByExpanding,
                                            Qt::SmoothTransformation));
  albumcover_label->setScaledContents(true);

  volume_slider_frame->resize(100, 40);
  volume_slider->setMinimum(0);
  volume_slider->setMaximum(100);

  connect(volume_pushButton, SIGNAL(clicked()), this, SLOT(showVolumeSlider()));
  QHBoxLayout *volumeslider = new QHBoxLayout(volume_slider_frame);
  volumeslider->addWidget(volume_slider);

  this->setMouseTracking(true);

  widget->installEventFilter(widget);

  // Start connection threads
  mpd.start();
  mpdDb.start();

  // Set connection data
  QSettings settings;
  settings.beginGroup("mpd-server-connection");
  Todi::hostname = settings.value("host").toString();
  Todi::port = static_cast<quint16>(settings.value("port").toUInt());
  settings.endGroup();
  mpd.setHostname(Todi::hostname);
  mpd.setPort(Todi::port);
  mpdDb.setHostname(Todi::hostname);
  mpdDb.setPort(Todi::port);

  while (!mpd.connectToMPD()) {
    qWarning() << "Retrying to connect...";
    if (!showMpdConnectionDialog()) {
      qCritical()
          << "MPD connection couldnot be established! Exiting application...";
      exit(EXIT_FAILURE);
    }
    mpd.setHostname(Todi::hostname);
    mpd.setPort(Todi::port);
    mpdDb.setHostname(Todi::hostname);
    mpdDb.setPort(Todi::port);
    qWarning() << "Unable to connect to MPD with Hostname : " << Todi::hostname
               << " Port : " << Todi::port;
  }

  // MPD
  connect(&mpd, SIGNAL(statsUpdated()), this, SLOT(updateStats()));
  connect(&mpd, SIGNAL(statusUpdated()), this, SLOT(updateStatus()),
          Qt::DirectConnection);

  auto quitApplication = []() { QApplication::quit(); };
  connect(quitAction, &QAction::triggered, quitApplication);
  connect(close_pushButton, &QPushButton::clicked, quitApplication);
  connect(about, &QAction::triggered, []() {
    std::unique_ptr<AboutDialog> abt(new AboutDialog());
    abt->exec();
  });
  connect(expand_collapse_PushButton, SIGNAL(clicked(bool)),
          SLOT(expandCollapse()));

  // Basic media buttons
  connect(play_pause_pushButton, SIGNAL(clicked(bool)), SLOT(playPauseTrack()));
  connect(previous_pushButton, &QPushButton::clicked, [&]() {
    mpd.goToPrevious();
    mpd.getStatus();
  });
  connect(next_pushButton, &QPushButton::clicked, [&]() {
    mpd.goToNext();
    mpd.getStatus();
  });

  // volume & track slider
  connect(track_slider, SIGNAL(sliderPressed()), this,
          SLOT(positionSliderPressed()));
  connect(track_slider, SIGNAL(sliderReleased()), this, SLOT(setPosition()));
  connect(track_slider, SIGNAL(sliderReleased()), this,
          SLOT(positionSliderReleased()));
  connect(volume_slider, &QSlider::valueChanged,
          [&](int vol) { mpd.setVolume(static_cast<quint8>(vol)); });
  track_slider->setTracking(true);
  track_slider->setTickInterval(0);

  // Timer time out update status
  statusTimer.start(settings.value("connection/interval", 1000).toInt());
  connect(&statusTimer, SIGNAL(timeout()), &mpd, SLOT(getStatus()));

  // update status & stats when starting the application
  mpd.getStatus();
  mpd.getStats();

  /* Check if we need to get a new db or not */
  if (!musicLibraryModel.fromXML(MPDStats::getInstance()->dbUpdate()))
    mpdDb.listAllInfo(MPDStats::getInstance()->dbUpdate());

  mpdDb.listAll();
}

QSize Player::sizeHint() const { return QSize(100, 100); }

Player::~Player() { delete ui_; }

void Player::mousePressEvent(QMouseEvent *event) {
  dragPosition = event->pos();
  QWidget::mousePressEvent(event);
  if (event->button() == Qt::LeftButton) {
    setCursor(Qt::SizeAllCursor);
  }
}

void Player::mouseReleaseEvent(QMouseEvent *event) {
  dragPosition = QPoint();
  QWidget::mouseReleaseEvent(event);
  if (event->button() == Qt::LeftButton) {
    setCursor(Qt::ArrowCursor);
  }
}

void Player::leaveEvent(QEvent *) {
  /*songMetadata_label->show();
  previous_pushButton->hide();
  play_pause_pushButton->hide();
  next_pushButton->hide();
  volume_pushButton->hide();
  track_slider->hide();
  update();*/
}

void Player::enterEvent(QEvent *) {}

void Player::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() & Qt::LeftButton) {
    if (resize_status) {
      QPoint p = mapToGlobal(event->pos()) - geometry().topLeft();
      resize(p.x(), p.y());
    } else {
      move(mapToParent(event->pos() - dragPosition));
    }
  } else {
    QPoint diff = geometry().bottomRight() - mapToGlobal(event->pos());
    if (diff.x() <= 10 && diff.y() <= 10) {
      if (!resize_status) {
        setCursor(Qt::SizeFDiagCursor);
        resize_status = true;
      }
    } else {
      if (resize_status) {
        setCursor(Qt::ArrowCursor);
        resize_status = false;
      }
    }
  }
}

void Player::resizeEvent(QResizeEvent *) {
  if (widget->width() > 200) {
    albumcover_label->show();
  } else {
    albumcover_label->hide();
  }
  if (widget->width() > 350) {
    search_pushButton->show();
    playlist_pushButton->show();
  } else {
    search_pushButton->hide();
    playlist_pushButton->hide();
  }
}

int Player::showMpdConnectionDialog() {
  std::unique_ptr<MpdConnectionDialog> connect(new MpdConnectionDialog(this));
  return connect->exec();
}

void Player::expandCollapse() {
  if (this->width() != this->height()) {
    this->setFixedHeight(this->width());
    expand_collapse_PushButton->setIcon(
        IconLoader::load("edit-collapse", IconLoader::LightDark));
  } else {
    this->setFixedHeight(61);
    // this->resize(this->width(), 61);
    expand_collapse_PushButton->setIcon(
        IconLoader::load("edit-expand", IconLoader::LightDark));
  }
}

void Player::showVolumeSlider() {
  // volume_slider_frame->setFrameStyle(3);
  // volume_slider_frame->setFrameShape(QFrame::StyledPanel);
  volume_slider_frame->show();
  // volume_slider_frame->move(volume_pushButton->x(), volume_pushButton->y());
  volume_slider_frame->move(
      mapToGlobal(volume_pushButton->geometry().topLeft()));
}

void Player::updateStats() {
  MPDStats *const stats = MPDStats::getInstance();

  /*
   * Check if remote db is more recent than local one
   * Also update the dirview
   */
  if (lastDbUpdate.isValid() && stats->dbUpdate() > lastDbUpdate) {
    mpdDb.listAllInfo(stats->dbUpdate());
    mpdDb.listAll();
  }

  lastDbUpdate = stats->dbUpdate();
}

void Player::updateStatus() {
  MPDStatus *const status = MPDStatus::getInstance();
  QString timeElapsedFormattedString;

  // Retrieve stats every 5 seconds
  fetchStatsFactor = (fetchStatsFactor + 1) % 5;
  if (fetchStatsFactor == 0) mpd.getStats();

  if (!draggingPositionSlider) {
    if (status->state() == MPDStatus::State::StateStopped ||
        status->state() == MPDStatus::State::StateInactive) {
      track_slider->setValue(0);
    } else {
      track_slider->setRange(0, status->timeTotal());
      track_slider->setValue(status->timeElapsed());
    }
  }

  volume_slider->setValue(status->volume());

  /*if (status->random())
    randomCheckBox->setCheckState(Qt::Checked);
  else
    randomCheckBox->setCheckState(Qt::Unchecked);

  if (status->repeat())
    repeatCheckBox->setCheckState(Qt::Checked);
  else
    repeatCheckBox->setCheckState(Qt::Unchecked);*/

  if (status->state() == MPDStatus::State::StateStopped ||
      status->state() == MPDStatus::State::StateInactive) {
    timeElapsedFormattedString = "00:00 / 00:00";
  } else {
    timeElapsedFormattedString +=
        Song::formattedTime(static_cast<quint32>(status->timeElapsed()));
    // timeElapsedFormattedString += " / ";
    // timeElapsedFormattedString +=
    //    Song::formattedTime(static_cast<quint32>(status->timeTotal()));
  }

  timer_label->setText(timeElapsedFormattedString);

  switch (status->state()) {
    case MPDStatus::State::StatePlaying:
      // Main window
      play_pause_pushButton->setIcon(
          IconLoader::load("media-playback-pause", IconLoader::LightDark));
      play_pause_pushButton->setEnabled(true);
      break;

    case MPDStatus::State::StateInactive:
    case MPDStatus::State::StateStopped:
      // Main window
      play_pause_pushButton->setIcon(
          IconLoader::load("media-playback-start", IconLoader::LightDark));
      play_pause_pushButton->setEnabled(true);
      /*trackTitleLabel->setText("");
      trackArtistLabel->setText("");
      trackAlbumLabel->setText("");
      albumCoverLabel->setPixmap(QPixmap());*/
      break;

    case MPDStatus::State::StatePaused:
      // Main window
      play_pause_pushButton->setIcon(
          IconLoader::load("media-playback-start", IconLoader::LightDark));
      play_pause_pushButton->setEnabled(true);
      break;
      qDebug("Invalid state");
  }

  // Check if song has changed or we're playing again after being stopped
  // and update song info if needed
  if (lastState == MPDStatus::State::StateInactive ||
      (lastState == MPDStatus::State::StateStopped &&
       status->state() == MPDStatus::State::StatePlaying) ||
      lastSongId != status->songId())
    mpd.currentSong();

  /* Update libre scrobbler stuff (no librefm support for now)*/
  /*if (settings.value("lastfm/enabled", "false").toBool()) {
    if (lastState == MPDStatus::State_Playing &&
        status->state() == MPDStatus::State_Paused) {
      emit pauseSong();
    } else if (lastState == MPDStatus::State_Paused &&
               status->state() == MPDStatus::State_Playing) {
      emit resumeSong();
      emit nowPlaying();
    } else if (lastState != MPDStatus::State_Stopped &&
               status->state() == MPDStatus::State_Stopped) {
      emit stopSong();
    }

    if (status->state() == MPDStatus::State_Playing) {
      nowPlayingFactor = (nowPlayingFactor + 1) % 20;
      if (nowPlayingFactor == 0) emit nowPlaying();
    }
  }*/

  // Set TrayIcon tooltip
  /*QString text = toolTipText;
  text += "time: " + timeElapsedFormattedString;
  if (trayIcon != NULL) trayIcon->setToolTip(text);*/

  // Check if playlist has changed and update if needed
  if (lastState == MPDStatus::State::StateInactive ||
      lastPlaylist < status->playlist()) {
    mpd.playListInfo();
  }

  // Display bitrate
  bitrateLabel.setText("Bitrate: " + QString::number(status->bitrate()));

  // Update status info
  lastState = status->state();
  lastSongId = status->songId();
  lastPlaylist = status->playlist();
}

void Player::playPauseTrack() {
  MPDStatus *const status = MPDStatus::getInstance();

  if (status->state() == MPDStatus::State::StatePlaying)
    mpd.setPause(true);
  else if (status->state() == MPDStatus::State::StatePaused)
    mpd.setPause(false);
  else
    mpd.startPlayingSong();

  mpd.getStatus();
}

void Player::stopTrack() {
  mpd.stopPlaying();
  mpd.getStatus();
}

void Player::positionSliderPressed() { draggingPositionSlider = true; }

void Player::setPosition() {
  mpd.setSeekId(static_cast<quint32>(MPDStatus::getInstance()->songId()),
                static_cast<quint32>(track_slider->value()));
}

void Player::positionSliderReleased() { draggingPositionSlider = false; }

void Player::setAlbumCover(QImage image, QString artist, QString album) {
  Q_UNUSED(artist);
  Q_UNUSED(album);

  qDebug("setalbum cover ()");
  if (image.isNull()) {
    albumcover_label->setText("No cover available");
    return;
  }

  // Display image
  QPixmap pixmap = QPixmap::fromImage(image);
  pixmap = pixmap.scaled(QSize(150, 150), Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
  albumcover_label->setPixmap(pixmap);

  // Save image to avoid downloading it next time
  /*QDir dir(QDir::home());
  if (!dir.exists(".Todi")) {
    if (!dir.mkdir(".Todi")) {
      qWarning("Couldn't create directory for storing album covers");
      return;
    }
  }

  dir.cd(".Todi");
  QString file(QFile::encodeName(artist + " - " + album + ".jpg"));
  image.save(dir.absolutePath() + QDir::separator() + file);*/
}