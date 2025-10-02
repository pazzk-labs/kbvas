# 한국환경공단 화재예방형 전기차 충전기 배터리 정보 교환 프로토콜 라이브러리

## Dependencies
- [`libmcu/base64.h`](https://github.com/libmcu/libmcu/blob/main/modules/common/include/libmcu/base64.h)

## Usage

```c
#define BATCH_MAXLEN  10

static void on_kbvas_receive(const void *data, size_t datasize) {
    kbvas_enqueue(kbvas, data, datasize);
}

static bool on_kbvas_iterate(struct kbvas *self, const struct kbvas_entry *entry, void *ctx) {
    return true;
}

void main(void) {
    struct kbvas_backend_api *backend = kbvas_memory_backend_create();
    struct kbvas *kbvas = kbvas_create(backend, NULL);
    kbvas_set_batch_count(kbvas, BATCH_MAXLEN);

    while (1) {
        if (kbvas_is_batch_ready(kbvas)) {
            kbvas_iterate(kbvas, on_kbvas_iterate, NULL);
            kbvas_clear_batch(kbvas);
        }
    }
}
```

## References
- [2024년 전기차 화재예방형 충전기 보조사업 공고 및 완속, 급속 지침](https://ev.or.kr/nportal/infoGarden/selectBBSListDtl.do?ARTC_ID=19182&BLBD_ID=guide)
- [2024년 전기자동차 완속충전시설 보조사업 보조금 및 설치 운영 지침](https://www.easylaw.go.kr/CSP/FlDownload.laf?flSeq=1713934332841#:~:text=%E2%80%9C%ED%99%94%EC%9E%AC%EC%98%88%EB%B0%A9%ED%98%95%20%EC%B6%A9%EC%A0%84%EA%B8%B0%E2%80%9D%EB%9E%80,%EA%B0%80%20%EA%B0%80%EB%8A%A5%ED%95%9C%20%EC%B6%A9%EC%A0%84%EA%B8%B0%EB%A5%BC%20%EB%A7%90%ED%95%9C%EB%8B%A4.&text=%EB%94%B0%EB%9D%BC%20%EC%84%A4%EC%B9%98%ED%95%9C%20%EC%A0%84%EC%82%B0%EB%A7%9D%EC%9D%84%20%EB%A7%90%ED%95%9C%EB%8B%A4.)
- [KTC 화재예방형 전기차 충전기 시험인증 안내](https://www.ktc.re.kr/web_united/board/view_season_02.asp?pagen=572&sno=3081)
