/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "stdafx.h"
#include <stdint.h>
#include <math.h>
#include "SkMaskGamma.h"

class SkGammaColorSpaceLuminance : public SkColorSpaceLuminance {
    SkScalar toLuma(SkScalar gamma, SkScalar luminance) const override {
        //return SkScalarPow(luminance, gamma);
		return 0;
    }

    SkScalar fromLuma(SkScalar gamma, SkScalar luma) const override {
        //return SkScalarPow(luma, SkScalarInvert(gamma));
		return 0;
    }
};

/*static*/ const SkColorSpaceLuminance& SkColorSpaceLuminance::Fetch(SkScalar gamma) {
    static SkGammaColorSpaceLuminance gSkGammaColorSpaceLuminance;
	return gSkGammaColorSpaceLuminance;
}

static float apply_contrast(float srca, float contrast) {
    return srca + ((1.0f - srca) * contrast * srca);
}

void SkTMaskGamma_build_correcting_lut(uint8_t table[256], U8CPU srcI, SkScalar contrast,
                                       const SkColorSpaceLuminance& srcConvert, SkScalar srcGamma,
                                       const SkColorSpaceLuminance& dstConvert, SkScalar dstGamma) {
    const float src = (float)srcI / 255.0f;
    const float linSrc = srcConvert.toLuma(srcGamma, src);
    //Guess at the dst. The perceptual inverse provides smaller visual
    //discontinuities when slight changes to desaturated colors cause a channel
    //to map to a different correcting lut with neighboring srcI.
    //See https://code.google.com/p/chromium/issues/detail?id=141425#c59 .
    const float dst = 1.0f - src;
    const float linDst = dstConvert.toLuma(dstGamma, dst);

    //Contrast value tapers off to 0 as the src luminance becomes white
    const float adjustedContrast = SkScalarToFloat(contrast) * linDst;

    //Remove discontinuity and instability when src is close to dst.
    //The value 1/256 is arbitrary and appears to contain the instability.
    if (fabs(src - dst) < (1.0f / 256.0f)) {
        float ii = 0.0f;
        for (int i = 0; i < 256; ++i, ii += 1.0f) {
            float rawSrca = ii / 255.0f;
            float srca = apply_contrast(rawSrca, adjustedContrast);
            table[i] = SkToU8(sk_float_round2int(255.0f * srca));
        }
    } else {
        // Avoid slow int to float conversion.
        float ii = 0.0f;
        for (int i = 0; i < 256; ++i, ii += 1.0f) {
            // 'rawSrca += 1.0f / 255.0f' and even
            // 'rawSrca = i * (1.0f / 255.0f)' can add up to more than 1.0f.
            // When this happens the table[255] == 0x0 instead of 0xff.
            // See http://code.google.com/p/chromium/issues/detail?id=146466
            float rawSrca = ii / 255.0f;
            float srca = apply_contrast(rawSrca, adjustedContrast);
            float dsta = 1.0f - srca;

            //Calculate the output we want.
            float linOut = (linSrc * srca + dsta * linDst);
            float out = dstConvert.fromLuma(dstGamma, linOut);

            //Undo what the blit blend will do.
            float result = (out - dst) / (src - dst);

            table[i] = SkToU8(sk_float_round2int(255.0f * result));
        }
    }
}
