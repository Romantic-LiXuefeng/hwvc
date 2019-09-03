/*
 * Copyright (c) 2018-present, lmyooyo@gmail.com.
 *
 * This source code is licensed under the GPL license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include "Logcat.h"
#include "../include/HwRender.h"
#include "../include/HwNormalFilter.h"
#include "../include/ObjectBox.h"
#include "TimeUtils.h"
#include "../include/HwRGBA2NV12Filter.h"
#include "../include/HwTexture.h"
#include "../include/HwFBObject.h"
#include <GLES2/gl2.h>

HwRender::HwRender(string alias) : Unit(alias) {
#ifdef ANDROID
    filter = new HwNormalFilter();
#else
    filter = new HwNormalFilter();
#endif
    yuvReadFilter = new HwRGBA2NV12Filter();
    registerEvent(EVENT_COMMON_PREPARE, reinterpret_cast<EventFunc>(&HwRender::eventPrepare));
    registerEvent(EVENT_COMMON_PIXELS_READ,
                  reinterpret_cast<EventFunc>(&HwRender::eventReadPixels));
    registerEvent(EVENT_RENDER_FILTER, reinterpret_cast<EventFunc>(&HwRender::eventRenderFilter));
    registerEvent(EVENT_RENDER_SET_FILTER, reinterpret_cast<EventFunc>(&HwRender::eventSetFilter));
}

HwRender::~HwRender() {
}

bool HwRender::eventPrepare(Message *msg) {
    Logcat::i("HWVC", "Render::eventPrepare");
    return true;
}

bool HwRender::eventRelease(Message *msg) {
    Logcat::i("HWVC", "Render::eventRelease");
    if (yuvReadFilter) {
        delete yuvReadFilter;
        yuvReadFilter = nullptr;
    }
    if (filter) {
        delete filter;
        Logcat::i("HWVC", "Render::eventRelease filter");
        filter = nullptr;
    }
    if (pixels) {
        delete[] pixels;
        pixels = nullptr;
    }
    if (buf) {
        delete buf;
        buf = nullptr;
    }
    if (yuvTarget) {
        delete yuvTarget;
        yuvTarget = nullptr;
    }
    if (target) {
        delete target;
        target = nullptr;
    }
    if (fbo) {
        delete fbo;
        fbo = nullptr;
    }
    return true;
}

bool HwRender::eventReadPixels(Message *msg) {
    bool read = false;
    fbo->bind();
    if (yuvReadFilter) {
        glViewport(0, 0, yuvTarget->getWidth(), yuvTarget->getHeight());
        yuvReadFilter->draw(target, yuvTarget);
        if (fbo->read(buf->data())) {
            read = true;
        }
    }
    if (!read && fbo->read(buf->data())) {
        read = true;
    }
    fbo->unbind();
    if (read) {
        Message *msg1 = new Message(EVENT_COMMON_PIXELS, nullptr);
        msg1->obj = HwBuffer::wrap(buf->data(), buf->size());
        msg1->arg2 = tsInNs;
        postEvent(msg1);
    }
    return true;
}

bool HwRender::eventRenderFilter(Message *msg) {
    Logcat::i("HWVC", "Render::eventFilter");
    HwTexture *tex = static_cast<HwTexture *>(msg->obj);
    tsInNs = msg->arg2;
    checkFilter(tex->getWidth(), tex->getHeight());
    glViewport(0, 0, tex->getWidth(), tex->getHeight());
    renderFilter(tex);
    notifyPixelsReady();
    renderScreen();
    return true;
}

bool HwRender::eventSetFilter(Message *msg) {
    Logcat::i("HWVC", "Render::eventSetFilter");
    HwAbsFilter *newFilter = static_cast<HwAbsFilter *>(msg->tyrUnBox());
    if (filter) {
        delete filter;
        filter = nullptr;
    }
    filter = newFilter;
    return true;
}

void HwRender::renderScreen() {
    Logcat::i("HWVC", "Render::renderScreen");
    Message *msg = new Message(EVENT_SCREEN_DRAW, nullptr);
    msg->obj = new ObjectBox(new Size(target->getWidth(), target->getHeight()));
    msg->arg1 = target->texId();
    postEvent(msg);
}

void HwRender::checkFilter(int width, int height) {
    if (filter) {
        bool ret = filter->prepare();
        if (yuvReadFilter) {
            yuvReadFilter->prepare();
            yuvTarget = HwTexture::alloc(GL_TEXTURE_2D);
            yuvTarget->update(nullptr, width / 4, height * 3 / 2);
            fbo = HwFBObject::alloc();
            fbo->bindTex(yuvTarget);
        }
        if (ret) {
            target = HwTexture::alloc(GL_TEXTURE_2D);
            target->update(nullptr, width, height);
            size_t size = static_cast<size_t>(width * height * 3 / 2);
            buf = HwBuffer::alloc(size);
        }
    }
    if (!pixels) {
        pixels = new uint8_t[width * height * 3 / 2];
    }
}

void HwRender::renderFilter(HwAbsTexture *tex) {
    Logcat::i("HWVC", "Render::renderFilter %d", tex->texId());
    filter->draw(tex, target);
#if 0
    if (yuvReadFilter) {
        yuvReadFilter->draw(filter->getFrameBuffer()->getFrameTexture());
    }
    //Test fbo read.
    ++count;
    if (count >= 150) {
        count = 0;
        int64_t time = TimeUtils::getCurrentTimeUS();
        yuvReadFilter->getFrameBuffer()->read(pixels);
        FILE *file = fopen("/sdcard/pixels.yv12", "wb");
        size_t size = yuvReadFilter->getFrameBuffer()->width()
                      * yuvReadFilter->getFrameBuffer()->height() * 4;
        Logcat::i("HWVC", "HwAndroidFrameBuffer::read cost %lld, %dx%d",
                  TimeUtils::getCurrentTimeUS() - time,
                  yuvReadFilter->getFrameBuffer()->width(),
                  yuvReadFilter->getFrameBuffer()->height());
        fwrite(pixels, 1, size, file);
        fclose(file);
    }
#endif
}

void HwRender::notifyPixelsReady() {
    postEvent(new Message(EVENT_COMMON_PIXELS_READY, nullptr, Message::QUEUE_MODE_FIRST_ALWAYS,
                          nullptr));
}