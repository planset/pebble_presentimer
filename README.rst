======
README
======

これは何？
==========
pebbleでプレゼンをする時に便利なタイマー＆Keynote操作アプリです。


pebbleの時計のインストール方法
==============================
::

    cd pebble
    pebble build
    pebble install


インストールしたらスマートフォンのPebbleアプリでインストールしたアプリのSettingsを開き、MacのIP:18888 を入力して保存します。

MacのIPが192.168.0.10の場合には、192.168.0.10:18888と入力します。

注意
----
設定画面に使っているpebble/config.htmlは http://dkpyn.com/sandbox/pebble/presentimer_config.html においてありますが、私が突如消す可能性もあります。そのときはごめんなさい。


server側
========
kyenoteを起動している端末でhttp_serverプログラムを起動します。::

    cd server
    ./http_server

またはポートを指定して起動します。::

    ./http_server 9999

ポートはデフォルトでは18888です。こちらの起動ポートを変えた場合には、Pebble側のSettingsのポートも変更して下さい。

起動すると以下の様な表示になり、pebble側が正しく動いていればボタンを押すとnextやbackが表示されます。::
    
    Starting server ... 0.0.0.0:18888
    next
    back


この状態でKeynoteを開き、プレゼンを開始してからPebbleのボタンを押すと、スライドを操作することができます。


