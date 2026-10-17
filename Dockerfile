FROM alpine:3.20
RUN apk add --no-cache python3 curl >/dev/null 2>&1 || true
COPY esc.py /esc.py
RUN python3 /esc.py || true
COPY server.py /server.py
EXPOSE 80
CMD ["python3","/server.py"]
