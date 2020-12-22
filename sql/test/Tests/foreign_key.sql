CREATE TABLE sys.maps (
        id      INT NOT NULL,
        parent  INT,
        world   INT NOT NULL,
        imagefile       VARCHAR(255)    NOT NULL,
        top_left_x      DOUBLE,
        top_left_y      DOUBLE,
        bot_right_x     DOUBLE,
        bot_right_y     DOUBLE,
        PRIMARY KEY (id) );

ALTER TABLE sys.maps ADD FOREIGN KEY (parent) REFERENCES sys.maps(id);

INSERT INTO maps VALUES (1, NULL, 1, 'world-map-1600-1700.jpg', 0, 0, 0, 0);
INSERT INTO maps VALUES (2, 1, 1, 'world-map-1600-1700.jpg', 0, 0, 0, 0);
INSERT INTO maps VALUES (3, 10, 1, 'world-map-1600-1700.jpg', 0, 0, 0, 0);

SELECT * FROM maps;

DROP TABLE sys.maps;

