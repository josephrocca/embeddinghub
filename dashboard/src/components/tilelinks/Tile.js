import React from "react";
import Icon from "@material-ui/core/Icon";
import { makeStyles } from "@material-ui/core/styles";
import { useHistory } from "react-router-dom";
import ButtonBase from "@material-ui/core/ButtonBase";
import Button from "@material-ui/core/Button";
import Box from "@material-ui/core/Box";
import Typography from "@material-ui/core/Typography";

const useStyles = makeStyles((theme, id) => ({
  root: {
    padding: theme.spacing(2),
    width: "100%",
    height: "100%",
  },
  card: {
    padding: theme.spacing(3),
    width: "100%",
    height: "100%",
    border: (id) => `2px solid ${theme.palette.ordinalColors[id % 4]}`,
    borderRadius: 16,
    background: "white",
    minWidth: "12em",
    textTransform: "none",
  },

  media: {
    height: 200,
  },

  button: {
    width: "100%",
    height: "100%",
  },
  buttonDisabled: {
    padding: theme.spacing(3),
    opacity: 0.3,
    width: "100%",
    height: "100%",
    border: (id) => `2px solid ${theme.palette.ordinalColors[id % 4]}`,
    borderRadius: 16,
    background: "white",
    textTransform: "none",
    minWidth: "12em",
  },
  icon: {
    marginRight: theme.spacing(2),
    color: (id) => theme.palette.ordinalColors[id % 4],
    fontSize: "2em",
  },
  title: {
    fontSize: "2em",
    color: (id) => theme.palette.ordinalColors[id % 4],
  },

  tileContent: {
    display: "flex",
    justifyContent: "space-around",
  },
}));

const Tile = ({ detail, id }) => {
  let history = useHistory();
  const disabled = detail.disabled;
  const classes = useStyles(id);

  let buttonClass = classes.card;

  if (disabled) {
    buttonClass = classes.buttonDisabled;
  }

  const handleClick = (event) => {
    history.push(detail.path);
  };

  return (
    <Box sx={{ boxShadow: 3 }}>
      <div className={classes.root}>
        <Button
          className={buttonClass}
          onClick={handleClick}
          disabled={disabled}
          focusRipple={true}
          variant="contained"
        >
          <div className={classes.tileContent}>
            <div>
              <Icon color="green" className={classes.icon}>
                {detail.icon}
              </Icon>
            </div>

            <div>
              <Typography className={classes.title} variant="h5">
                <b>{detail.title}</b>
              </Typography>
            </div>
          </div>
        </Button>
      </div>
    </Box>
  );
};

export default Tile;
